#include "internal.hpp"

#include <algorithm>
#include <cctype>

#include <hex/api/content_registry/file_type_handler.hpp>
#include <hex/api/imhex_api/provider.hpp>
#include <hex/api/project_manager.hpp>
#include <hex/api/task_manager.hpp>
#include <hex/api/events/requests_interaction.hpp>
#include <hex/helpers/fmt.hpp>
#include <hex/helpers/fs.hpp>
#include <hex/helpers/logger.hpp>
#include <hex/providers/file_backed_provider_data.hpp>
#include <hex/providers/provider.hpp>
#include <hex/ui/imgui_imhex_extensions.h>

#include <fonts/fonts.hpp>
#include <fonts/tabler_icons.hpp>
#include <fonts/vscode_icons.hpp>
#include <popups/popup_question.hpp>
#include <toasts/toast_notification.hpp>

#include <wolv/utils/string.hpp>

#include <imgui_internal.h>

namespace hex::plugin::builtin::project::impl {

    namespace {

        enum class InlineEditMode {
            CreateFile,
            CreateFolder,
            Rename
        };

        struct InlineEdit {
            InlineEditMode mode;
            std::fs::path directory;
            std::fs::path source;
            std::string typeId;
            std::string name;
            bool focus = true;
        };

        std::optional<InlineEdit> s_inlineEdit;

        FileBackedProviderDataBase *getDataTypeForFile(const std::fs::path &path) {
            auto extension = path.extension().string();
            if (extension.starts_with('.'))
                extension.erase(extension.begin());

            for (auto *data : FileBackedProviderDataRegistry::getTypes()) {
                for (const auto &filter : data->getType().extensions) {
                    if (filter.spec == extension)
                        return data;
                }
            }
            return nullptr;
        }

        void persistAssociations() {
            std::ignore = storeAssociations(ProjectManager::getProjectRoot());
        }

        bool isReservedProjectPath(const std::fs::path &path) {
            return std::ranges::any_of(path, [](const auto &part) { return part == ProjectManager::ProjectDirectory; });
        }

        bool isValidEntryName(const std::string &name) {
            const auto path = std::fs::path(name);
            return !name.empty() && path != "." && path != ".." && path == path.filename() && path != ProjectManager::ProjectDirectory;
        }

        bool isSameOrDescendant(const std::fs::path &path, const std::fs::path &base) {
            const auto normalizedPath = path.lexically_normal();
            const auto normalizedBase = base.lexically_normal();
            if (normalizedPath == normalizedBase)
                return true;

            const auto relative = normalizedPath.lexically_relative(normalizedBase);
            return !relative.empty() && !relative.is_absolute() &&
                   std::ranges::none_of(relative, [](const auto &part) { return part == ".."; });
        }

        std::fs::path remapProjectPath(const std::fs::path &path, const std::fs::path &source, const std::fs::path &destination) {
            if (path.lexically_normal() == source.lexically_normal())
                return destination.lexically_normal();
            return (destination / path.lexically_normal().lexically_relative(source.lexically_normal())).lexically_normal();
        }

        bool containsProviderFile(const std::fs::path &relativePath) {
            const auto root = ProjectManager::getProjectRoot();
            std::error_code error;
            const auto canonicalRoot = std::fs::weakly_canonical(root, error);
            if (error)
                return false;

            for (const auto &lockedFilePath : prv::IProviderFileBacked::getAllLockedFiles()) {
                error.clear();
                const auto providerRelativePath = std::fs::weakly_canonical(lockedFilePath, error).lexically_relative(canonicalRoot);
                if (!error && isSameOrDescendant(providerRelativePath, relativePath))
                    return true;
            }

            return false;
        }

        void showProjectFileError(const UnlocalizedString &message, const UnlocalizedString &detailedMessage,
                                  const std::fs::path &path, const std::error_code &error = {}) {
            if (error)
                ui::ToastError::open(fmt::format(Lang(detailedMessage), path.generic_string(), error.message()));
            else
                ui::ToastError::open(fmt::format(Lang(message), path.generic_string()));
        }

        bool moveProjectEntry(const std::fs::path &source, const std::fs::path &destination) {
            if (!isSafeProjectPath(source) || !isSafeProjectPath(destination) ||
                isReservedProjectPath(source) || isReservedProjectPath(destination) || source == destination)
                return false;

            const auto root = ProjectManager::getProjectRoot();
            const auto sourcePath = root / source;
            const auto destinationPath = root / destination;
            std::error_code error;
            if (!std::fs::exists(sourcePath, error) || std::fs::exists(destinationPath, error) || error) {
                showProjectFileError("hex.builtin.popup.error.project.entry.move"_unlocalized, "hex.builtin.popup.error.project.entry.move.details"_unlocalized, source, error);
                return false;
            }
            if (std::fs::is_directory(sourcePath, error) && isSameOrDescendant(destination, source)) {
                ui::ToastError::open(fmt::format("hex.builtin.popup.error.project.entry.move_into_self"_lang, source.generic_string()));
                return false;
            }
            if (containsProviderFile(source)) {
                ui::ToastError::open("hex.builtin.popup.error.project.entry.backing_file_in_use"_lang);
                return false;
            }

            struct BindingRelocation {
                prv::Provider *provider;
                std::string typeId;
                std::fs::path path;
            };
            std::vector<BindingRelocation> relocations;
            auto updatedAssociations = state().associations;
            for (auto &[providerId, entries] : updatedAssociations) {
                auto *provider = getProviderById(providerId);
                for (auto &[typeId, path] : entries) {
                    if (!isSameOrDescendant(path, source))
                        continue;

                    const auto oldPath = path;
                    path = remapProjectPath(path, source, destination);
                    if (provider != nullptr) {
                        const auto binding = FileBackedProviderDataRegistry::getBinding(provider, typeId);
                        if (binding.has_value() && binding->lexically_normal() == (root / oldPath).lexically_normal())
                            relocations.push_back({ provider, typeId, root / path });
                    }
                }
            }

            std::fs::rename(sourcePath, destinationPath, error);
            if (error) {
                showProjectFileError("hex.builtin.popup.error.project.entry.move"_unlocalized, "hex.builtin.popup.error.project.entry.move.details"_unlocalized, source, error);
                return false;
            }

            for (const auto &[provider, typeId, path] : relocations) {
                if (!FileBackedProviderDataRegistry::relocate(provider, typeId, path))
                    log::error("Failed to relocate provider data binding '{}' to '{}'", typeId, path.string());
            }
            state().associations = std::move(updatedAssociations);
            persistAssociations();
            return true;
        }

        bool deleteProjectEntry(const std::fs::path &relativePath) {
            if (!isSafeProjectPath(relativePath) || isReservedProjectPath(relativePath) || containsProviderFile(relativePath)) {
                showProjectFileError("hex.builtin.popup.error.project.entry.delete"_unlocalized, "hex.builtin.popup.error.project.entry.delete.details"_unlocalized, relativePath);
                return false;
            }

            const auto path = ProjectManager::getProjectRoot() / relativePath;
            std::error_code error;
            if (std::fs::is_directory(path, error))
                std::fs::remove_all(path, error);
            else
                std::fs::remove(path, error);
            if (error) {
                showProjectFileError("hex.builtin.popup.error.project.entry.delete"_unlocalized, "hex.builtin.popup.error.project.entry.delete.details"_unlocalized, relativePath, error);
                return false;
            }

            std::vector<std::pair<prv::Provider *, std::string>> bindings;
            auto &associations = state().associations;
            for (auto providerIt = associations.begin(); providerIt != associations.end();) {
                auto *provider = getProviderById(providerIt->first);
                auto &entries = providerIt->second;
                for (auto entry = entries.begin(); entry != entries.end();) {
                    if (isSameOrDescendant(entry->second, relativePath)) {
                        if (provider != nullptr)
                            bindings.emplace_back(provider, entry->first);
                        entry = entries.erase(entry);
                    } else {
                        ++entry;
                    }
                }
                if (entries.empty())
                    providerIt = associations.erase(providerIt);
                else
                    ++providerIt;
            }

            for (const auto &[provider, typeId] : bindings)
                std::ignore = FileBackedProviderDataRegistry::unbind(provider, typeId);
            persistAssociations();
            return true;
        }

        void startCreatingFolder(const std::fs::path &directory) {
            s_inlineEdit = InlineEdit {
                .mode = InlineEditMode::CreateFolder,
                .directory = directory,
                .source = {},
                .typeId = {},
                .name = {}
            };
        }

        void startCreatingFile(const std::fs::path &directory, std::string typeId) {
            s_inlineEdit = InlineEdit {
                .mode = InlineEditMode::CreateFile,
                .directory = directory,
                .source = {},
                .typeId = std::move(typeId),
                .name = {}
            };
        }

        void startRenamingProjectEntry(const std::fs::path &relativePath) {
            s_inlineEdit = InlineEdit {
                .mode = InlineEditMode::Rename,
                .directory = relativePath.parent_path(),
                .source = relativePath,
                .typeId = {},
                .name = relativePath.filename().string()
            };
        }

        void commitInlineEdit() {
            if (!s_inlineEdit.has_value() || !isValidEntryName(s_inlineEdit->name)) {
                if (s_inlineEdit.has_value())
                    ui::ToastError::open(fmt::format("hex.builtin.popup.error.project.entry.invalid_name"_lang, s_inlineEdit->name));
                return;
            }

            auto edit = std::move(*s_inlineEdit);
            s_inlineEdit.reset();
            TaskManager::doLater([edit = std::move(edit)] {
                if (edit.mode == InlineEditMode::CreateFolder) {
                    const auto relativePath = (edit.directory / edit.name).lexically_normal();
                    if (!isSafeProjectPath(relativePath) || isReservedProjectPath(relativePath))
                        return;

                    std::error_code error;
                    if (!std::fs::create_directory(ProjectManager::getProjectRoot() / relativePath, error))
                        showProjectFileError("hex.builtin.popup.error.project.entry.create_folder"_unlocalized, "hex.builtin.popup.error.project.entry.create_folder.details"_unlocalized, relativePath, error);
                } else if (edit.mode == InlineEditMode::CreateFile) {
                    auto *data = FileBackedProviderDataRegistry::get(edit.typeId);
                    if (data == nullptr || data->getType().extensions.empty())
                        return;

                    auto fileName = std::fs::path(edit.name);
                    if (!fileName.has_extension())
                        fileName += fmt::format(".{}", data->getType().extensions.front().spec);
                    const auto relativePath = (edit.directory / fileName).lexically_normal();
                    if (!isSafeProjectPath(relativePath) || isReservedProjectPath(relativePath) ||
                        !FileBackedProviderDataRegistry::createFile(edit.typeId, ProjectManager::getProjectRoot() / relativePath))
                        showProjectFileError("hex.builtin.popup.error.project.entry.create_file"_unlocalized, "hex.builtin.popup.error.project.entry.create_file.details"_unlocalized, relativePath);
                } else {
                    auto fileName = std::fs::path(edit.name);
                    std::error_code error;
                    if (std::fs::is_regular_file(ProjectManager::getProjectRoot() / edit.source, error) && !fileName.has_extension())
                        fileName += edit.source.extension();
                    moveProjectEntry(edit.source, edit.directory / fileName);
                }
            });
        }

        void drawInlineEditField(const char *icon, bool drawIcon = true) {
            ImGui::PushID("ProjectInlineEdit");
            if (drawIcon) {
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetTreeNodeToLabelSpacing());
                ImGui::TextUnformatted(icon);
                ImGui::SameLine();
            }
            ImGui::SetNextItemWidth(-FLT_MIN);
            if (s_inlineEdit->focus) {
                ImGui::SetKeyboardFocusHere();
                s_inlineEdit->focus = false;
            }

            ImGui::PushStyleVarY(ImGuiStyleVar_FramePadding, 0);
            const bool submitted = ImGui::InputText("##Name", s_inlineEdit->name,
                ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);
            const bool commitAfterDeactivation = ImGui::IsItemDeactivated();
            if (ImGui::IsKeyPressed(ImGuiKey_Escape))
                s_inlineEdit.reset();
            else if (submitted || commitAfterDeactivation)
                commitInlineEdit();
            ImGui::PopStyleVar();
            ImGui::PopID();
        }

        void drawInlineCreation(const std::fs::path &directory) {
            if (!s_inlineEdit.has_value() || s_inlineEdit->mode == InlineEditMode::Rename || s_inlineEdit->directory != directory)
                return;

            if (s_inlineEdit->mode == InlineEditMode::CreateFolder) {
                drawInlineEditField(ICON_VS_FOLDER);
            } else if (auto *data = FileBackedProviderDataRegistry::get(s_inlineEdit->typeId); data != nullptr) {
                drawInlineEditField(data->getType().displayIcon.c_str());
            }
        }

        void promptDeleteProjectEntry(const std::fs::path &relativePath) {
            ui::PopupQuestion::open(UntranslatedString(fmt::format("hex.builtin.popup.project.delete_entry.confirm"_lang, relativePath.generic_string())),
                [relativePath] { deleteProjectEntry(relativePath); }, [] { });
        }

        void drawCreateProjectEntryMenu(const std::fs::path &directory) {
            if (ImGui::BeginMenuEx("hex.builtin.sidebar.project.menu.new_file"_lang, ICON_VS_NEW_FILE)) {
                for (auto *data : FileBackedProviderDataRegistry::getTypes()) {
                    if (data->getType().extensions.empty())
                        continue;
                    if (ImGui::MenuItemEx(Lang(data->getType().displayName), data->getType().displayIcon.c_str()))
                        startCreatingFile(directory, data->getType().typeId);
                }
                ImGui::EndMenu();
            }
            if (ImGui::MenuItemEx("hex.builtin.sidebar.project.menu.new_folder"_lang, ICON_VS_NEW_FOLDER))
                startCreatingFolder(directory);
        }

        void removeAssociation(prv::Provider *provider, const std::string &handlerPath) {
            if (provider == nullptr)
                return;

            auto &associations = state().associations;
            auto providerAssociations = associations.find(provider->getID());
            if (providerAssociations == associations.end())
                return;
            const auto association = providerAssociations->second.find(handlerPath);
            if (association == providerAssociations->second.end())
                return;

            std::ignore = FileBackedProviderDataRegistry::unbind(provider, handlerPath);

            providerAssociations->second.erase(association);
            if (providerAssociations->second.empty())
                associations.erase(providerAssociations);
            persistAssociations();
        }

        const char *getProjectFileIcon(const std::fs::path &path) {
            if (const auto *data = getDataTypeForFile(path); data != nullptr)
                return data->getType().displayIcon.c_str();

            const auto extension = path.extension().string();
            for (const auto &[extensions, handler, icon] : ContentRegistry::FileTypeHandler::impl::getEntries()) {
                std::ignore = handler;
                if (!icon.empty() && std::ranges::contains(extensions, extension))
                    return icon.c_str();
            }
            return ICON_VS_FILE;
        }

        bool associateFile(prv::Provider *provider, const std::fs::path &relativePath) {
            auto *data = getDataTypeForFile(relativePath);
            if (provider == nullptr || data == nullptr || !isSafeProjectPath(relativePath))
                return false;

            const auto typeId = data->getType().typeId;
            if (data->bind(provider, ProjectManager::getProjectRoot() / relativePath)) {
                state().associations[provider->getID()][typeId] = relativePath;
                persistAssociations();
                return true;
            }
            return false;
        }

        void drawProjectEntryDragSource(const std::fs::path &relativePath, const char *icon) {
            if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceNoHoldToOpenOthers)) {
                const auto payload = relativePath.generic_string();
                ImGui::SetDragDropPayload("IMHEX_PROJECT_ENTRY", payload.c_str(), payload.size() + 1);
                ImGuiExt::TextFormatted("{} {}", icon, relativePath.generic_string());
                ImGui::EndDragDropSource();
            }
        }

        void drawProjectDirectoryDropTarget(const std::fs::path &directory) {
            if (!ImGui::BeginDragDropTarget())
                return;

            if (const auto *payload = ImGui::AcceptDragDropPayload("IMHEX_PROJECT_ENTRY"); payload != nullptr) {
                const auto source = std::fs::path(static_cast<const char *>(payload->Data));
                const auto destination = (directory / source.filename()).lexically_normal();
                if (source != destination)
                    TaskManager::doLater([source, destination] { moveProjectEntry(source, destination); });
            }
            ImGui::EndDragDropTarget();
        }

        void drawProjectFile(const std::fs::path &root, const std::fs::path &path) {
            std::error_code error;
            const auto relativePath = std::fs::relative(path, root, error);
            if (error)
                return;

            ImGui::PushID(relativePath.generic_string().c_str());
            if (s_inlineEdit.has_value() && s_inlineEdit->mode == InlineEditMode::Rename && s_inlineEdit->source == relativePath) {
                drawInlineEditField(getProjectFileIcon(path));
                ImGui::PopID();
                return;
            }

            const auto label = fmt::format("{}  {}", getProjectFileIcon(path), path.filename().string());
            ImGui::TreeNodeEx(label.c_str(), ImGuiTreeNodeFlags_DrawLinesToNodes | ImGuiTreeNodeFlags_SpanFullWidth |
                ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen);
            const bool fileHovered = ImGui::IsItemHovered();

            if (fileHovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                auto *provider = ImHexApi::Provider::get();
                if (provider == nullptr || !associateFile(provider, relativePath))
                    RequestOpenFile::post(path);
            }

            drawProjectEntryDragSource(relativePath, getProjectFileIcon(path));

            if (fileHovered && ImGui::IsMouseReleased(ImGuiMouseButton_Right))
                ImGui::OpenPopup("ProjectFileContextMenu");
            if (ImGui::BeginPopup("ProjectFileContextMenu")) {
                if (ImGui::MenuItemEx("hex.builtin.sidebar.project.menu.rename"_lang, ICON_VS_EDIT))
                    startRenamingProjectEntry(relativePath);
                if (ImGui::MenuItemEx("hex.builtin.sidebar.project.menu.delete"_lang, ICON_VS_TRASH))
                    promptDeleteProjectEntry(relativePath);
                ImGui::Separator();
                if (ImGui::MenuItemEx("hex.builtin.sidebar.project.menu.reveal_in_file_manager"_lang, ICON_VS_FOLDER_OPENED))
                    fs::openFolderWithSelectionExternal(path);
                ImGui::EndPopup();
            }
            ImGui::PopID();
        }

        void drawProjectDirectory(const std::fs::path &root, const std::fs::path &directory) {
            std::error_code relativeError;
            auto directoryRelativePath = std::fs::relative(directory, root, relativeError);
            if (relativeError || directory == root)
                directoryRelativePath.clear();
            drawInlineCreation(directoryRelativePath);

            std::vector<std::fs::directory_entry> entries;
            std::error_code error;
            for (std::fs::directory_iterator iterator(directory, std::fs::directory_options::skip_permission_denied, error), end; iterator != end && !error; iterator.increment(error)) {
                if (iterator->path().filename() != ProjectManager::ProjectDirectory && !iterator->is_symlink(error))
                    entries.push_back(*iterator);
            }
            std::ranges::sort(entries, [](const auto &left, const auto &right) {
                return std::pair(!left.is_directory(), left.path().filename().string()) <
                       std::pair(!right.is_directory(), right.path().filename().string());
            });

            for (const auto &entry : entries) {
                if (entry.is_directory(error)) {
                    ImGui::PushID(entry.path().string().c_str());
                    const auto relativePath = std::fs::relative(entry.path(), root, error);
                    const bool renaming = s_inlineEdit.has_value() && s_inlineEdit->mode == InlineEditMode::Rename && s_inlineEdit->source == relativePath;
                    const bool creatingChild = s_inlineEdit.has_value() && s_inlineEdit->mode != InlineEditMode::Rename && s_inlineEdit->directory == relativePath;
                    if (creatingChild)
                        ImGui::SetNextItemOpen(true);

                    bool open;
                    if (renaming) {
                        open = ImGui::TreeNodeEx("##RenamingDirectory", ImGuiTreeNodeFlags_DrawLinesToNodes | ImGuiTreeNodeFlags_OpenOnArrow, "%s", ICON_VS_FOLDER);
                        ImGui::SameLine();
                        drawInlineEditField(ICON_VS_FOLDER, false);
                    } else {
                        const auto label = fmt::format("{}  {}", ICON_VS_FOLDER, entry.path().filename().string());
                        open = ImGui::TreeNodeEx(label.c_str(), ImGuiTreeNodeFlags_DrawLinesToNodes |
                            ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanFullWidth);
                    }
                    if (!renaming) {
                        drawProjectEntryDragSource(relativePath, ICON_VS_FOLDER);
                        drawProjectDirectoryDropTarget(relativePath);
                        if (ImGui::BeginPopupContextItem("##ProjectDirectoryContextMenu")) {
                            drawCreateProjectEntryMenu(relativePath);
                            ImGui::Separator();
                            if (ImGui::MenuItemEx("hex.builtin.sidebar.project.menu.rename"_lang, ICON_VS_EDIT))
                                startRenamingProjectEntry(relativePath);
                            if (ImGui::MenuItemEx("hex.builtin.sidebar.project.menu.delete"_lang, ICON_VS_TRASH))
                                promptDeleteProjectEntry(relativePath);
                            ImGui::EndPopup();
                        }
                    }
                    if (open) {
                        drawProjectDirectory(root, entry.path());
                        ImGui::TreePop();
                    }
                    ImGui::PopID();
                } else if (entry.is_regular_file(error)) {
                    drawProjectFile(root, entry.path());
                }
            }
        }

    }

    void resetSidebarState() {
        s_inlineEdit.reset();
    }

    void drawProjectSidebar() {
        if (!ProjectManager::isFolderProject())
            return;

        if (ImGui::BeginChild("##ProjectTree", ImVec2(0, 0), ImGuiChildFlags_Borders)) {
            const auto providers = ImHexApi::Provider::getProviders();
            const auto projectRoot = ProjectManager::getProjectRoot();
            const auto projectName = ProjectManager::isTemporaryProject()
                ? std::string("hex.builtin.sidebar.project.temporary"_lang.get())
                : wolv::util::toUTF8String(projectRoot.filename());
            const auto projectLabel = fmt::format("{}  {}", ICON_VS_PROJECT, projectName);
            ImGui::SetNextItemOpen(true, ImGuiCond_Once);
            const bool projectOpen = ImGui::TreeNodeEx("##Project",
                ImGuiTreeNodeFlags_DrawLinesToNodes | ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanFullWidth,
                "%s", projectLabel.c_str());

            if (!ProjectManager::isTemporaryProject()) {
                ImGui::SameLine();
                fonts::Default().pushItalic();
                ImGui::TextDisabled("%s", wolv::util::toUTF8String(projectRoot).c_str());
                fonts::Default().pop();
            }

            auto &projectState = state();
            if (projectOpen) {
                std::optional<u32> providerToOpen;
                std::optional<u32> providerToReplace;
                std::optional<u32> providerToRemove;
                std::optional<u32> providerToClose;

                std::set<u32> openProviderIds;
                for (auto *provider : providers) {
                    openProviderIds.insert(provider->getID());
                    if (!canPersistProvider(provider))
                        continue;
                    const bool partOfProject = projectState.projectProviderIds.contains(provider->getID());

                    ImGui::PushID(provider);
                    const auto associations = projectState.associations.find(provider->getID());
                    const bool hasAssociations = associations != projectState.associations.end() && !associations->second.empty();
                    auto flags = ImGuiTreeNodeFlags_DrawLinesToNodes | ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_OpenOnArrow;
                    if (provider == ImHexApi::Provider::get())
                        flags |= ImGuiTreeNodeFlags_Selected;
                    if (!hasAssociations)
                        flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;

                    const auto providerLabel = fmt::format("{}  {}", partOfProject ? provider->getIcon() : ICON_TA_FILE_BROKEN, provider->getName());
                    const bool open = ImGui::TreeNodeEx("##Provider", flags, "%s", providerLabel.c_str());
                    if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
                        ImHexApi::Provider::setCurrentProvider(provider);
                    if (ImGui::BeginPopupContextItem("##ProviderContextMenu")) {
                        if (partOfProject) {
                            if (ImGui::MenuItemEx("hex.builtin.sidebar.project.menu.remove_from_project"_lang, ICON_VS_TRASH))
                                providerToRemove = provider->getID();
                        } else {
                            if (ImGui::MenuItemEx("hex.builtin.sidebar.project.menu.add_to_project"_lang, ICON_VS_ADD))
                                projectState.projectProviderIds.insert(provider->getID());
                        }

                        if (ImGui::MenuItemEx("hex.ui.common.close"_lang, ICON_VS_CLOSE))
                            providerToClose = provider->getID();
                        ImGui::EndPopup();
                    }
                    if (ImGui::BeginDragDropTarget()) {
                        if (const auto *payload = ImGui::AcceptDragDropPayload("IMHEX_PROJECT_ENTRY"); payload != nullptr) {
                            const auto relativePath = std::fs::path(static_cast<const char *>(payload->Data));
                            associateFile(provider, relativePath);
                        }
                        ImGui::EndDragDropTarget();
                    }

                    if (open && hasAssociations) {
                        std::optional<std::string> associationToRemove;
                        for (const auto &[handlerPath, filePath] : associations->second) {
                            ImGui::PushID(handlerPath.c_str());
                            const auto label = fmt::format("{}  {}", getProjectFileIcon(filePath), filePath.filename().string());
                            ImGui::TreeNodeEx(label.c_str(), ImGuiTreeNodeFlags_DrawLinesToNodes | ImGuiTreeNodeFlags_SpanFullWidth |
                                ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen);
                            if (ImGui::IsItemHovered())
                                ImGui::SetTooltip("%s", filePath.generic_string().c_str());
                            if (ImGui::BeginPopupContextItem()) {
                                if (ImGui::MenuItemEx("hex.builtin.sidebar.project.menu.remove_from_provider"_lang, ICON_VS_DEBUG_DISCONNECT))
                                    associationToRemove = handlerPath;
                                ImGui::EndPopup();
                            }
                            ImGui::PopID();
                        }
                        ImGui::TreePop();
                        if (associationToRemove.has_value())
                            removeAssociation(provider, *associationToRemove);
                    }
                    ImGui::PopID();
                }

                for (const auto id : projectState.projectProviderIds) {
                    if (openProviderIds.contains(id))
                        continue;

                    ImGui::PushID(id);
                    const auto info = getStoredProviderDisplayInfo(id);
                    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
                    ImGui::TreeNodeEx("##ClosedProvider", ImGuiTreeNodeFlags_DrawLinesToNodes | ImGuiTreeNodeFlags_SpanFullWidth |
                        ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen, "%s  %s", info.icon, info.name.c_str());
                    ImGui::PopStyleColor();
                    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                        providerToOpen = id;
                    if (ImGui::BeginPopupContextItem("##ClosedProviderContextMenu")) {
                        if (ImGui::MenuItemEx("hex.ui.common.open"_lang, ICON_VS_GO_TO_FILE))
                            providerToOpen = id;
                        if (ImGui::MenuItemEx("hex.builtin.sidebar.project.menu.update_source"_lang, ICON_VS_REPLACE))
                            providerToReplace = id;
                        if (ImGui::MenuItemEx("hex.builtin.sidebar.project.menu.remove_from_project"_lang, ICON_VS_TRASH))
                            providerToRemove = id;
                        ImGui::EndPopup();
                    }
                    ImGui::PopID();
                }

                if (providerToOpen.has_value()) {
                    TaskManager::doLater([id = *providerToOpen] {
                        if (!openStoredProjectProvider(id))
                            ui::ToastError::open(fmt::format("hex.builtin.popup.error.project.provider.open"_lang, getStoredProviderName(id)));
                    });
                }
                if (providerToReplace.has_value()) {
                    TaskManager::doLater([id = *providerToReplace] {
                        if (!reopenProviderWithNewSettings(id))
                            ui::ToastError::open(fmt::format("hex.builtin.popup.error.project.provider.open"_lang, getStoredProviderName(id)));
                    });
                }
                if (providerToRemove.has_value())
                    TaskManager::doLater([id = *providerToRemove] { removeProviderFromProject(id); });
                if (providerToClose.has_value())
                    TaskManager::doLater([id = *providerToClose] { ImHexApi::Provider::remove(getProviderById(id)); });

                ImGui::TreePop();
            }

            ImGui::SetNextItemOpen(true, ImGuiCond_Once);
            if (s_inlineEdit.has_value() && s_inlineEdit->mode != InlineEditMode::Rename && s_inlineEdit->directory.empty())
                ImGui::SetNextItemOpen(true);
            const auto filesLabel = fmt::format("{}  {}", ICON_VS_ROOT_FOLDER, "hex.builtin.sidebar.project.files"_lang);
            const bool filesOpen = ImGui::TreeNodeEx("##ProjectFiles", ImGuiTreeNodeFlags_DrawLinesToNodes |
                ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanFullWidth, "%s", filesLabel.c_str());
            drawProjectDirectoryDropTarget({});
            if (ImGui::BeginPopupContextItem("##ProjectFilesContextMenu")) {
                drawCreateProjectEntryMenu({});
                ImGui::EndPopup();
            }
            if (filesOpen) {
                drawProjectDirectory(ProjectManager::getProjectRoot(), ProjectManager::getProjectRoot());
                ImGui::TreePop();
            }
        }
        ImGui::EndChild();
    }

}
