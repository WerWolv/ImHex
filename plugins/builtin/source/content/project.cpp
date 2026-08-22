#include <filesystem>
#include <algorithm>
#include <cctype>
#include <map>
#include <set>

#include <content/project.hpp>

#include <wolv/utils/guards.hpp>
#include <wolv/utils/string.hpp>

#include <hex/api/content_registry/user_interface.hpp>
#include <hex/api/content_registry/file_type_handler.hpp>
#include <hex/api/content_registry/provider.hpp>
#include <hex/api/imhex_api/provider.hpp>
#include <hex/api/project_manager.hpp>
#include <hex/api/localization_manager.hpp>
#include <hex/api/achievement_manager.hpp>
#include <hex/api/events/events_lifecycle.hpp>
#include <hex/api/events/events_provider.hpp>
#include <hex/api/events/requests_interaction.hpp>
#include <hex/api/events/requests_gui.hpp>
#include <hex/api/task_manager.hpp>

#include <hex/providers/provider.hpp>
#include <hex/providers/file_backed_provider_data.hpp>
#include <hex/ui/imgui_imhex_extensions.h>
#include <imgui_internal.h>

#include <hex/helpers/fmt.hpp>
#include <hex/helpers/fs.hpp>
#include <hex/helpers/logger.hpp>

#include <fonts/fonts.hpp>
#include <fonts/vscode_icons.hpp>
#include <fonts/tabler_icons.hpp>

#include <toasts/toast_notification.hpp>
#include <popups/popup_question.hpp>
#include <wolv/io/file.hpp>
#include <nlohmann/json.hpp>


namespace hex::plugin::builtin {

    const static auto ProjectSettingsPath = fmt::format("{}/project.json", ProjectManager::ProjectDirectory);

    static bool reopenProviderWithNewSettings(u32 id);

    namespace {

        using Associations = std::map<u32, std::map<std::string, std::fs::path>>;
        using ProviderSettings = std::map<u32, std::string>;
        Associations s_associations;
        std::set<u32> s_projectProviderIds;
        std::set<u32> s_closedProjectProviderIds;
        std::set<u32> s_providerOpenAttempts;
        std::map<const prv::Provider *, u32> s_providerReplacements;
        ProviderSettings s_projectProviderSettings;
        bool s_loadingProject = false;

        std::string getProviderName(const nlohmann::json &descriptor) {
            if (const auto settings = descriptor.find("settings"); settings != descriptor.end() && settings->is_object()) {
                for (const auto *key : { "displayName", "name" }) {
                    if (const auto value = settings->find(key); value != settings->end() && value->is_string() && !value->empty())
                        return value->get<std::string>();
                }

                if (const auto path = settings->find("path"); path != settings->end() && path->is_string() && !path->empty())
                    return wolv::util::toUTF8String(std::fs::path(path->get<std::string>()).filename());
            }

            if (const auto type = descriptor.find("type"); type != descriptor.end() && type->is_string())
                return Lang(type->get<std::string>());

            return "hex.builtin.sidebar.project.provider_fallback"_lang;
        }

        std::string getStoredProviderName(u32 id) {
            const auto storedSettings = s_projectProviderSettings.find(id);
            if (storedSettings == s_projectProviderSettings.end())
                return "hex.builtin.sidebar.project.provider_fallback"_lang;

            try {
                return getProviderName(nlohmann::json::parse(storedSettings->second));
            } catch (const std::exception &) {
                return "hex.builtin.sidebar.project.provider_fallback"_lang;
            }
        }

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

        bool isSafeProjectPath(const std::fs::path &path) {
            if (path.empty() || path.is_absolute())
                return false;

            if (std::ranges::any_of(path, [](const auto &part) { return part == ".."; }))
                return false;

            std::error_code error;
            const auto root = std::fs::weakly_canonical(ProjectManager::getProjectRoot(), error);
            const auto resolvedPath = std::fs::weakly_canonical(root / path, error);
            if (error)
                return false;

            const auto relativePath = resolvedPath.lexically_relative(root);
            return !relativePath.empty() && !relativePath.is_absolute() &&
                   std::ranges::none_of(relativePath, [](const auto &part) { return part == ".."; });
        }

        std::string readProjectFile(const std::fs::path &root, const std::fs::path &path) {
            return wolv::io::File(root / path, wolv::io::File::Mode::Read).readString();
        }

        bool writeProjectFile(const std::fs::path &root, const std::fs::path &path, const std::string &content) {
            std::error_code error;
            std::fs::create_directories((root / path).parent_path(), error);
            if (error)
                return false;
            wolv::io::File file(root / path, wolv::io::File::Mode::Create);
            return file.isValid() && file.writeString(content) == static_cast<i64>(content.size()) && file.flush();
        }

        void loadAssociations(const std::fs::path &root) {
            s_associations.clear();
            if (!std::fs::is_regular_file(root / ProjectSettingsPath))
                return;

            try {
                const auto settings = nlohmann::json::parse(readProjectFile(root, ProjectSettingsPath));
                const auto association = settings.value("associations", nlohmann::json::object());
                for (const auto &[providerId, entries] : association.items()) {
                    for (const auto &[handler, pathValue] : entries.items()) {
                        auto path = std::fs::path(pathValue.get<std::string>());
                        if (isSafeProjectPath(path) && !path.generic_string().starts_with(".imhex/"))
                            s_associations[std::stoul(providerId)][handler] = std::move(path);
                    }
                }
            } catch (const std::exception &error) {
                s_associations.clear();
                log::warn("Failed to load project associations: {}", error.what());
            }
        }

        bool storeAssociations(const std::fs::path &root) {
            nlohmann::json associations = nlohmann::json::object();
            for (const auto &[providerId, entries] : s_associations) {
                for (const auto &[handler, path] : entries)
                    associations[std::to_string(providerId)][handler] = path.generic_string();
            }

            return writeProjectFile(root, ProjectSettingsPath, nlohmann::json({
                { "version", 1 },
                { "associations", std::move(associations) }
            }).dump(4));
        }

        std::string rebaseStoredProviderSettings(const std::string &serializedSettings,
                                                  const std::fs::path &sourceRoot,
                                                  const std::fs::path &destinationRoot) {
            if (sourceRoot.empty() || sourceRoot.lexically_normal() == destinationRoot.lexically_normal())
                return serializedSettings;

            static const std::set<std::string> PathProviderTypes = {
                "hex.builtin.provider.file",
                "hex.builtin.provider.base64",
            };

            try {
                auto descriptor = nlohmann::json::parse(serializedSettings);
                if (!PathProviderTypes.contains(descriptor.at("type").get<std::string>()))
                    return serializedSettings;

                auto &settings = descriptor.at("settings");
                if (!settings.contains("path") || !settings["path"].is_string())
                    return serializedSettings;

                auto path = std::fs::path(settings["path"].get<std::string>());
                if (path.is_absolute())
                    return serializedSettings;

                std::error_code error;
                const auto rebasedPath = std::fs::proximate(sourceRoot / path, destinationRoot, error);
                if (error)
                    return serializedSettings;

                settings["path"] = wolv::io::fs::toNormalizedPathString(rebasedPath);
                return descriptor.dump(4);
            } catch (const std::exception &error) {
                log::warn("Failed to rebase stored project provider settings: {}", error.what());
                return serializedSettings;
            }
        }

        void snapshotProviderSettings(prv::Provider *provider) {
            try {
                s_projectProviderSettings[provider->getID()] = nlohmann::json({
                    { "type", provider->getTypeName() },
                    { "settings", provider->storeSettings({}) }
                }).dump(4);
            } catch (const std::exception &error) {
                log::warn("Failed to snapshot project provider {}: {}", provider->getID(), error.what());
            }
        }

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

        std::string getProviderFileStem(prv::Provider *provider) {
            auto name = provider->getName();
            for (auto &character : name) {
                const auto value = static_cast<unsigned char>(character);
                if (!std::isalnum(value) && character != '-' && character != '_')
                    character = '-';
            }
            if (name.empty())
                name = "provider";
            return fmt::format("{}-{}", name, provider->getID());
        }

        void persistAssociations() {
            storeAssociations(ProjectManager::getProjectRoot());
        }

        prv::Provider *getProviderById(u32 id) {
            const auto providers = ImHexApi::Provider::getProviders();
            const auto provider = std::ranges::find_if(providers, [id](const auto *entry) {
                return entry->getID() == id;
            });
            return provider == providers.end() ? nullptr : *provider;
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
                showProjectFileError("hex.builtin.popup.error.project.entry.move", "hex.builtin.popup.error.project.entry.move.details", source, error);
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
            auto updatedAssociations = s_associations;
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
                showProjectFileError("hex.builtin.popup.error.project.entry.move", "hex.builtin.popup.error.project.entry.move.details", source, error);
                return false;
            }

            for (const auto &[provider, typeId, path] : relocations) {
                if (!FileBackedProviderDataRegistry::relocate(provider, typeId, path))
                    log::error("Failed to relocate provider data binding '{}' to '{}'", typeId, path.string());
            }
            s_associations = std::move(updatedAssociations);
            persistAssociations();
            return true;
        }

        bool deleteProjectEntry(const std::fs::path &relativePath) {
            if (!isSafeProjectPath(relativePath) || isReservedProjectPath(relativePath) || containsProviderFile(relativePath)) {
                showProjectFileError("hex.builtin.popup.error.project.entry.delete", "hex.builtin.popup.error.project.entry.delete.details", relativePath);
                return false;
            }

            const auto path = ProjectManager::getProjectRoot() / relativePath;
            std::error_code error;
            if (std::fs::is_directory(path, error))
                std::fs::remove_all(path, error);
            else
                std::fs::remove(path, error);
            if (error) {
                showProjectFileError("hex.builtin.popup.error.project.entry.delete", "hex.builtin.popup.error.project.entry.delete.details", relativePath, error);
                return false;
            }

            std::vector<std::pair<prv::Provider *, std::string>> bindings;
            for (auto providerIt = s_associations.begin(); providerIt != s_associations.end();) {
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
                    providerIt = s_associations.erase(providerIt);
                else
                    ++providerIt;
            }

            for (const auto &[provider, typeId] : bindings) {
                std::ignore = FileBackedProviderDataRegistry::unbind(provider, typeId);
            }
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
                        showProjectFileError("hex.builtin.popup.error.project.entry.create_folder", "hex.builtin.popup.error.project.entry.create_folder.details", relativePath, error);
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
                        showProjectFileError("hex.builtin.popup.error.project.entry.create_file", "hex.builtin.popup.error.project.entry.create_file.details", relativePath);
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
            ui::PopupQuestion::open(fmt::format("hex.builtin.popup.project.delete_entry.confirm"_lang, relativePath.generic_string()),
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
            if (ImGui::MenuItemEx( "hex.builtin.sidebar.project.menu.new_folder"_lang, ICON_VS_NEW_FOLDER))
                startCreatingFolder(directory);
        }

        void removeAssociation(prv::Provider *provider, const std::string &handlerPath) {
            if (provider == nullptr)
                return;

            auto providerAssociations = s_associations.find(provider->getID());
            if (providerAssociations == s_associations.end())
                return;
            const auto association = providerAssociations->second.find(handlerPath);
            if (association == providerAssociations->second.end())
                return;

            std::ignore = FileBackedProviderDataRegistry::unbind(provider, handlerPath);

            providerAssociations->second.erase(association);
            if (providerAssociations->second.empty())
                s_associations.erase(providerAssociations);
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
                s_associations[provider->getID()][typeId] = relativePath;
                persistAssociations();
                return true;
            }
            return false;
        }

        void bindRegisteredData() {
            const auto root = ProjectManager::getProjectRoot();
            for (auto *provider : ImHexApi::Provider::getProviders()) {
                const auto providerAssociations = s_associations.find(provider->getID());
                if (providerAssociations == s_associations.end())
                    continue;

                for (const auto &[typeId, relativePath] : providerAssociations->second) {
                    if (FileBackedProviderDataRegistry::get(typeId) != nullptr)
                        std::ignore = FileBackedProviderDataRegistry::bind(provider, typeId, root / relativePath);
                }
            }
        }

        void materializeRegisteredData() {
            const auto root = ProjectManager::getProjectRoot();
            bool associationsChanged = false;
            for (auto *provider : ImHexApi::Provider::getProviders()) {
                if (!s_projectProviderIds.contains(provider->getID()))
                    continue;

                for (auto *data : FileBackedProviderDataRegistry::getTypes()) {
                    const auto &type = data->getType();
                    if (data->isBound(provider) || !data->hasPendingData(provider) || type.extensions.empty())
                        continue;

                    const auto extension = type.extensions.front().spec;
                    const auto relativePath = std::fs::path(fmt::format("{}.{}", getProviderFileStem(provider), extension));
                    if (data->bind(provider, root / relativePath)) {
                        s_associations[provider->getID()][type.typeId] = relativePath;
                        associationsChanged = true;
                    }
                }
            }
            if (associationsChanged)
                persistAssociations();
        }

        void removeProviderForProjectLoad(prv::Provider *provider) {
            ImHexApi::Provider::remove(provider, true);
        }

        bool openStoredProjectProvider(u32 id) {
            if (auto *provider = getProviderById(id); provider != nullptr) {
                ImHexApi::Provider::setCurrentProvider(provider);
                return true;
            }

            const auto storedSettings = s_projectProviderSettings.find(id);
            if (storedSettings == s_projectProviderSettings.end())
                return false;

            s_providerOpenAttempts.insert(id);
            auto finishOpenAttempt = SCOPE_GUARD { s_providerOpenAttempts.erase(id); };

            try {
                const auto providerSettings = nlohmann::json::parse(storedSettings->second);
                const auto providerType = providerSettings.at("type").get<std::string>();
                auto provider = ImHexApi::Provider::createProvider(providerType, true, true);
                if (provider == nullptr)
                    return false;

                provider->setID(id);
                provider->loadSettings(providerSettings.at("settings"));
                const auto result = provider->open();
                if (result.isFailure() || result.isRedirecting() || !provider->isAvailable() || !provider->isReadable()) {
                    removeProviderForProjectLoad(provider.get());
                    return false;
                }

                EventProviderOpened::post(provider.get());
                bindRegisteredData();
                return true;
            } catch (const std::exception &error) {
                log::warn("Failed to reopen project provider {}: {}", id, error.what());
                if (auto *provider = getProviderById(id); provider != nullptr)
                    removeProviderForProjectLoad(provider);
                return false;
            }
        }

        bool isProviderReplacement(const prv::Provider *provider) {
            return s_providerReplacements.contains(provider);
        }

        struct StoredProviderDisplayInfo {
            std::string name;
            const char *icon = ICON_VS_FILE_BINARY;
        };

        StoredProviderDisplayInfo getStoredProviderDisplayInfo(u32 id) {
            StoredProviderDisplayInfo result { .name = getStoredProviderName(id) };
            const auto storedSettings = s_projectProviderSettings.find(id);
            if (storedSettings == s_projectProviderSettings.end())
                return result;

            try {
                const auto providerSettings = nlohmann::json::parse(storedSettings->second);
                const auto providerType = providerSettings.at("type").get<std::string>();
                for (const auto &entry : ContentRegistry::Provider::impl::getEntries()) {
                    if (entry.unlocalizedName.get() == providerType) {
                        result.icon = entry.icon;
                        break;
                    }
                }
            } catch (const std::exception &) {
            }
            return result;
        }

        bool loadProviders(const std::fs::path &root) {
            const auto basePath = std::fs::path(ProjectManager::ProjectDirectory) / "providers";
            std::vector<u32> providerIds;
            std::vector<u32> closedProviderIds;
            try {
                const auto manifest = nlohmann::json::parse(readProjectFile(root, basePath / "providers.json"));
                providerIds = manifest.at("providers").get<std::vector<u32>>();
                closedProviderIds = manifest.value("closedProviders", std::vector<u32> {});

                const std::set<u32> providerIdSet(providerIds.begin(), providerIds.end());
                const std::set<u32> closedProviderIdSet(closedProviderIds.begin(), closedProviderIds.end());
                if (providerIdSet.size() != providerIds.size() || closedProviderIdSet.size() != closedProviderIds.size())
                    throw std::runtime_error("Project data source manifest contains duplicate entries");
                if (std::ranges::any_of(providerIds, [](u32 id) { return id >= std::numeric_limits<u32>::max() - 1; }))
                    throw std::runtime_error("Project data source manifest contains an invalid entry");
                if (!std::ranges::all_of(closedProviderIdSet, [&providerIdSet](u32 id) { return providerIdSet.contains(id); }))
                    throw std::runtime_error("Closed provider is not part of the project");
            } catch (const std::exception &error) {
                log::error("Failed to load project provider manifest at {}: {}", root.string(), error.what());
                ui::ToastError::open(fmt::format("hex.builtin.popup.error.project.load"_lang, error.what()));
                return false;
            }

            s_projectProviderIds = { providerIds.begin(), providerIds.end() };
            s_closedProjectProviderIds = { closedProviderIds.begin(), closedProviderIds.end() };
            s_projectProviderSettings.clear();

            for (const auto id : providerIds)
                prv::Provider::reserveID(id);

            if (providerIds.empty())
                return true;

            std::vector<std::string> failedProviderNames;
            for (const auto id : providerIds) {
                auto providerName = getStoredProviderName(id);
                try {
                    auto serializedSettings = readProjectFile(root, basePath / fmt::format("{}.json", id));
                    if (!serializedSettings.empty())
                        s_projectProviderSettings[id] = serializedSettings;
                    const auto providerSettings = nlohmann::json::parse(std::move(serializedSettings));
                    providerName = getProviderName(providerSettings);
                    const auto providerType = providerSettings.at("type").get<std::string>();
                    if (s_closedProjectProviderIds.contains(id))
                        continue;

                    auto provider = ImHexApi::Provider::createProvider(providerType, true, false);
                    if (provider == nullptr) {
                        log::warn("Failed to create project provider {} of type {}", id, providerType);
                        failedProviderNames.push_back(providerName);
                        continue;
                    }

                    provider->setID(id);
                    provider->loadSettings(providerSettings.at("settings"));
                    providerName = provider->getName();
                    const auto result = provider->open();
                    if (result.isFailure() || result.isRedirecting() || !provider->isAvailable() || !provider->isReadable()) {
                        removeProviderForProjectLoad(provider.get());
                        failedProviderNames.push_back(providerName);
                        continue;
                    }
                    EventProviderOpened::post(provider.get());
                } catch (const std::exception &error) {
                    log::warn("Failed to load project provider {}: {}", id, error.what());
                    if (auto *provider = getProviderById(id); provider != nullptr) {
                        providerName = provider->getName();
                        removeProviderForProjectLoad(provider);
                    }
                    failedProviderNames.push_back(providerName);
                }
            }

            if (!failedProviderNames.empty()) {
                ui::ToastWarning::open(fmt::format(
                    "hex.builtin.popup.error.project.load.some_providers_failed"_lang,
                    fmt::join(failedProviderNames, ", ")));
            }

            return true;
        }

        bool storeProviders(const std::fs::path &root, const std::fs::path &sourceRoot, ProviderSettings &storedSettings) {
            const auto basePath = std::fs::path(ProjectManager::ProjectDirectory) / "providers";
            auto providerSettings = s_projectProviderSettings;
            std::set<u32> serializedActiveProviderIds;
            for (const auto *provider : ImHexApi::Provider::getProviders()) {
                if (!s_projectProviderIds.contains(provider->getID()))
                    continue;
                if (isProviderReplacement(provider))
                    continue;

                serializedActiveProviderIds.insert(provider->getID());
                providerSettings[provider->getID()] = nlohmann::json({
                    { "type", provider->getTypeName() },
                    { "settings", provider->storeSettings({}) }
                }).dump(4);
            }

            for (const auto id : s_projectProviderIds) {
                auto settings = providerSettings.find(id);
                if (settings == providerSettings.end())
                    continue;

                if (!serializedActiveProviderIds.contains(id))
                    settings->second = rebaseStoredProviderSettings(settings->second, sourceRoot, root);
                if (!writeProjectFile(root, basePath / fmt::format("{}.json", id), settings->second))
                    return false;
            }

            if (!writeProjectFile(root, basePath / "providers.json", nlohmann::json({
                { "providers", s_projectProviderIds },
                { "closedProviders", s_closedProjectProviderIds }
            }).dump(4)))
                return false;

            storedSettings = std::move(providerSettings);
            return true;
        }

        void scheduleProjectMetadataSave();

        void removeProviderFromProject(u32 id) {
            auto *provider = getProviderById(id);
            s_projectProviderIds.erase(id);
            s_closedProjectProviderIds.erase(id);
            s_projectProviderSettings.erase(id);

            if (const auto associations = s_associations.find(id); associations != s_associations.end()) {
                std::vector<std::string> typeIds;
                for (const auto &[typeId, path] : associations->second) {
                    std::ignore = path;
                    typeIds.push_back(typeId);
                }
                s_associations.erase(associations);

                if (provider != nullptr) {
                    for (const auto &typeId : typeIds)
                        std::ignore = FileBackedProviderDataRegistry::unbind(provider, typeId);
                }
            }

            std::error_code error;
            std::fs::remove(ProjectManager::getProjectRoot() / ProjectManager::ProjectDirectory / "providers" /
                fmt::format("{}.json", id), error);
            if (error)
                log::warn("Failed to remove project provider metadata: {}", error.message());

            persistAssociations();
            scheduleProjectMetadataSave();
        }

        bool persistProjectMetadata() {
            if (s_loadingProject || !ProjectManager::isFolderProject())
                return false;

            const auto root = ProjectManager::getProjectRoot();
            ProviderSettings storedSettings;
            if (!storeProviders(root, root, storedSettings) || !storeAssociations(root)) {
                log::error("Failed to update project metadata at {}", root.string());
                return false;
            }
            s_projectProviderSettings = std::move(storedSettings);
            return true;
        }

        void scheduleProjectMetadataSave() {
            TaskManager::doLaterOnce([] {
                std::ignore = persistProjectMetadata();
            });
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
                if (ImGui::MenuItemEx( "hex.builtin.sidebar.project.menu.delete"_lang, ICON_VS_TRASH))
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

        void drawProjectSidebar() {
            if (!ProjectManager::isFolderProject())
                return;

            if (ImGui::BeginChild("##ProjectTree", ImVec2(0, 0), ImGuiChildFlags_Borders)) {
                const auto providers = ImHexApi::Provider::getProviders();
                const auto projectRoot = ProjectManager::getProjectRoot();
                const auto projectLabel = fmt::format("{}  {}", ICON_VS_PROJECT, ProjectManager::isDefaultProject() ? "" : wolv::util::toUTF8String(projectRoot.filename()));
                ImGui::SetNextItemOpen(true, ImGuiCond_Once);
                const bool projectOpen = ImGui::TreeNodeEx("##Project",
                    ImGuiTreeNodeFlags_DrawLinesToNodes | ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanFullWidth,
                    "%s", projectLabel.c_str()
                );

                if (!ProjectManager::isDefaultProject()) {
                    ImGui::SameLine();
                    fonts::Default().pushItalic();
                    ImGui::TextDisabled("%s", wolv::util::toUTF8String(projectRoot).c_str());
                    fonts::Default().pop();
                }

                if (projectOpen) {
                    std::optional<u32> providerToOpen;
                    std::optional<u32> providerToReplace;
                    std::optional<u32> providerToRemove;
                    std::optional<u32> providerToClose;

                    std::set<u32> openProviderIds;
                    for (auto *provider : providers) {
                        const bool partOfProject = s_projectProviderIds.contains(provider->getID());

                        openProviderIds.insert(provider->getID());

                        ImGui::PushID(provider);
                        const auto associations = s_associations.find(provider->getID());
                        const bool hasAssociations = associations != s_associations.end() && !associations->second.empty();
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
                                // Provider is part of project already

                                if (ImGui::MenuItemEx("hex.builtin.sidebar.project.menu.remove_from_project"_lang, ICON_VS_TRASH))
                                    providerToRemove = provider->getID();
                            } else {
                                // Provider is open but not part of the project

                                if (ImGui::MenuItemEx( "hex.builtin.sidebar.project.menu.add_to_project"_lang, ICON_VS_ADD))
                                    s_projectProviderIds.insert(provider->getID());
                            }

                            if (ImGui::MenuItemEx( "hex.ui.common.close"_lang, ICON_VS_CLOSE))
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

                    for (const auto id : s_projectProviderIds) {
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

    bool store(std::optional<std::fs::path> filePath, bool updateLocation);

    bool load(const std::fs::path &filePath) {
        s_inlineEdit.reset();
        if (!wolv::io::fs::isDirectory(filePath)) {
            ui::ToastError::open(fmt::format("hex.builtin.popup.error.project.load"_lang,
                fmt::format("hex.builtin.popup.error.project.load.file_not_found"_lang,
                    wolv::util::toUTF8String(filePath)
            )));

            return false;
        }

        const auto projectMetadataPath = std::fs::path(ProjectSettingsPath);
        if (!std::fs::is_regular_file(filePath / projectMetadataPath)) {
            s_associations.clear();
            s_projectProviderIds.clear();
            s_closedProjectProviderIds.clear();
            s_projectProviderSettings.clear();
            for (const auto *provider : ImHexApi::Provider::getProviders())
                s_projectProviderIds.insert(provider->getID());

            std::error_code error;
            std::fs::create_directories(filePath / ProjectManager::ProjectDirectory, error);
            if (error) {
                log::error("Failed to create project metadata directory at {}: {}", (filePath / ProjectManager::ProjectDirectory).string(), error.message());
                return false;
            }

            ProjectManager::setPath(filePath);
            ProjectManager::setFolderProject(true);
            materializeRegisteredData();
            return store(std::nullopt, true);
        }

        auto originalPath = ProjectManager::getPath();
        s_loadingProject = true;
        auto resetLoading = SCOPE_GUARD { s_loadingProject = false; };
        for (const auto &provider : ImHexApi::Provider::getProviders())
            removeProviderForProjectLoad(provider);

        ProjectManager::setPath(filePath);
        auto resetPath = SCOPE_GUARD {
            ProjectManager::setPath(originalPath);
        };

        if (!loadProviders(filePath))
            return false;

        loadAssociations(filePath);
        bindRegisteredData();

        resetLoading.release();
        s_loadingProject = false;
        resetPath.release();
        EventProjectOpened::post();
        RequestUpdateWindowTitle::post();

        return true;
    }
    
    bool store(std::optional<std::fs::path> filePath = std::nullopt, bool updateLocation = true) {
        auto originalPath = ProjectManager::getPath();
        const auto originalFolderProject = ProjectManager::isFolderProject();

        if (!filePath.has_value())
            filePath = originalPath;

        std::error_code directoryError;
        std::fs::create_directories(*filePath, directoryError);
        if (directoryError)
            return false;
        ProjectManager::setPath(filePath.value());
        ProjectManager::setFolderProject(true);
        auto resetPath = SCOPE_GUARD {
            ProjectManager::setPath(originalPath);
            ProjectManager::setFolderProject(originalFolderProject);
        };

        if (!originalFolderProject) {
            s_projectProviderIds.clear();
            s_closedProjectProviderIds.clear();
            s_projectProviderSettings.clear();
            for (const auto *provider : ImHexApi::Provider::getProviders())
                s_projectProviderIds.insert(provider->getID());
        }

        materializeRegisteredData();

        ProviderSettings storedSettings;
        bool result = storeProviders(*filePath, originalPath, storedSettings) && storeAssociations(*filePath);

        for (auto *data : FileBackedProviderDataRegistry::getTypes())
            result = data->flush() && result;

        if (!result)
            log::error("Failed to write project storage at {}", filePath->string());
        if (!result)
            return false;

        if (updateLocation)
            s_projectProviderSettings = std::move(storedSettings);

        ImHexApi::Provider::resetDataDirty();
        for (const auto &provider : ImHexApi::Provider::getProviders())
            provider->markMetadataDirty(false);

        // If saveLocation is false, reset the project path (do not release the lock)
        if (updateLocation) {
            resetPath.release();

            // Request, as this puts us into a project state
            RequestUpdateWindowTitle::post();

            AchievementManager::unlockAchievement("hex.builtin.achievement.starting_out", "hex.builtin.achievement.starting_out.save_project.name");

            EventProjectSaved::post();
        }

        return result;
    }

    std::string project::createEmptyProject(const std::filesystem::path &path) {
        if (path.empty())
            return "hex.builtin.popup.error.project.create.no_folder_selected"_lang;

        const auto metadataDirectory = path / ProjectManager::ProjectDirectory;
        std::error_code error;
        std::filesystem::create_directories(path, error);
        if (error)
            return fmt::format("hex.builtin.popup.error.project.create.folder_failed"_lang, error.message());
        if (!std::filesystem::is_directory(path, error))
            return "hex.builtin.popup.error.project.create.not_directory"_lang;

        if (!std::filesystem::create_directory(metadataDirectory, error)) {
            if (error)
                return fmt::format("hex.builtin.popup.error.project.create.metadata_failed"_lang, error.message());
            return "hex.builtin.popup.error.project.create.metadata_exists"_lang;
        }

        if (!std::filesystem::create_directory(metadataDirectory / "providers", error)) {
            std::error_code cleanupError;
            std::filesystem::remove(metadataDirectory, cleanupError);
            if (error)
                return fmt::format("hex.builtin.popup.error.project.create.metadata_failed"_lang, error.message());
            return "hex.builtin.popup.error.project.create.metadata_directory_exists"_lang;
        }

        const bool initialized = writeProjectFile(path, ProjectSettingsPath, nlohmann::json({
                { "version", 1 },
                { "associations", nlohmann::json::object() }
            }).dump(4)) &&
            writeProjectFile(path, std::filesystem::path(ProjectManager::ProjectDirectory) / "providers/providers.json", nlohmann::json({
                { "providers", nlohmann::json::array() },
                { "closedProviders", nlohmann::json::array() }
            }).dump(4));
        if (!initialized) {
            std::filesystem::remove_all(metadataDirectory, error);
            return "hex.builtin.popup.error.project.create.initialize_failed"_lang;
        }

        if (!load(path))
            return "hex.builtin.popup.error.project.create.open_failed"_lang;

        return {};
    }

    static bool reopenProviderWithNewSettings(u32 id) {
        if (!ProjectManager::isFolderProject() || !s_projectProviderIds.contains(id) ||
            getProviderById(id) != nullptr ||
            std::ranges::any_of(s_providerReplacements, [id](const auto &entry) { return entry.second == id; }) ||
            TaskManager::getRunningTaskCount() != 0)
            return false;

        const auto storedSettings = s_projectProviderSettings.find(id);
        if (storedSettings == s_projectProviderSettings.end())
            return false;

        try {
            const auto descriptor = nlohmann::json::parse(storedSettings->second);
            const auto providerType = descriptor.at("type").get<std::string>();
            auto provider = ImHexApi::Provider::createProvider(providerType);
            if (provider == nullptr || !std::ranges::contains(ImHexApi::Provider::getProviders(), provider.get()))
                return false;

            s_providerReplacements[provider.get()] = id;
            return true;
        } catch (const std::exception &error) {
            log::warn("Failed to reconfigure project provider {}: {}", id, error.what());
            return false;
        }
    }

    project::ImportResult project::importProviders(std::vector<ImportedProvider> providers, std::vector<ImportedProjectFile> projectFiles) {
        ImportResult result;
        if (!ProjectManager::isFolderProject()) {
            result.error = "hex.builtin.popup.error.project.import_legacy.no_open_project"_lang.get();
            return result;
        }

        std::set<u32> usedIds = s_projectProviderIds;
        for (const auto *provider : ImHexApi::Provider::getProviders())
            usedIds.insert(provider->getID());

        std::set<u32> legacyIds;
        std::map<u32, u32> remappedIds;
        u32 nextId = 0;
        for (const auto &provider : providers) {
            const auto providerName = getProviderName(provider.descriptor);
            if (!legacyIds.insert(provider.id).second) {
                result.error = fmt::format("hex.builtin.popup.error.project.import_legacy.duplicate_provider_id"_lang, providerName);
                return result;
            }
            if (!provider.descriptor.contains("type") || !provider.descriptor["type"].is_string() ||
                !provider.descriptor.contains("settings") || !provider.descriptor["settings"].is_object()) {
                result.error = fmt::format("hex.builtin.popup.error.project.import_legacy.invalid_provider_settings"_lang, providerName);
                return result;
            }

            auto id = provider.id;
            if (id >= std::numeric_limits<u32>::max() - 1) {
                result.error = fmt::format("hex.builtin.popup.error.project.import_legacy.reserved_provider_id"_lang, providerName);
                return result;
            }
            if (usedIds.contains(id)) {
                while (usedIds.contains(nextId) && nextId < std::numeric_limits<u32>::max() - 1)
                    nextId += 1;
                if (nextId >= std::numeric_limits<u32>::max() - 1) {
                    result.error = fmt::format("hex.builtin.popup.error.project.import_legacy.no_provider_ids_available"_lang, providerName);
                    return result;
                }
                id = nextId++;
            }
            usedIds.insert(id);
            remappedIds[provider.id] = id;
        }

        for (auto &provider : providers) {
            if (provider.descriptor["type"] == "hex.builtin.provider.view") {
                auto &settings = provider.descriptor["settings"];
                if (settings.contains("id")) {
                    const auto sourceId = settings["id"].get<u32>();
                    if (const auto remapped = remappedIds.find(sourceId); remapped != remappedIds.end())
                        settings["id"] = remapped->second;
                }
            }
            provider.id = remappedIds.at(provider.id);
            prv::Provider::reserveID(provider.id);
        }

        const auto root = ProjectManager::getProjectRoot();
        struct PreparedFile {
            u32 providerId;
            std::string typeId;
            std::fs::path relativePath;
        };
        std::vector<PreparedFile> preparedFiles;
        std::vector<std::fs::path> preparedProjectFiles;
        std::set<std::fs::path> reservedPaths;
        for (const auto &importedProvider : providers) {
            auto stem = fmt::format("provider-{}", importedProvider.id);
            if (const auto settings = importedProvider.descriptor.find("settings"); settings != importedProvider.descriptor.end()) {
                auto name = settings->value("displayName", std::string());
                if (name.empty())
                    name = settings->value("name", std::string());
                for (auto &character : name) {
                    const auto value = static_cast<unsigned char>(character);
                    if (!std::isalnum(value) && character != '-' && character != '_')
                        character = '-';
                }
                if (!name.empty())
                    stem = fmt::format("{}-{}", name, importedProvider.id);
            }

            for (const auto &file : importedProvider.files) {
                if (file.typeId.empty() || file.extension.empty())
                    continue;

                auto relativePath = std::fs::path(fmt::format("{}.{}", stem, file.extension));
                for (u32 suffix = 2; ; suffix += 1) {
                    std::error_code statusError;
                    const auto status = std::fs::symlink_status(root / relativePath, statusError);
                    if (statusError && statusError != std::errc::no_such_file_or_directory) {
                        result.error = fmt::format("hex.builtin.popup.error.project.import_legacy.inspect_path_failed"_lang,
                            relativePath.generic_string(), statusError.message());
                        return result;
                    }
                    if (status.type() == std::fs::file_type::not_found && !reservedPaths.contains(relativePath))
                        break;
                    relativePath = std::fs::path(fmt::format("{}-{}.{}", stem, suffix, file.extension));
                }
                reservedPaths.insert(relativePath);

                wolv::io::File output(root / relativePath, wolv::io::File::Mode::Create);
                if (!output.isValid() || output.writeVector(file.contents) != static_cast<i64>(file.contents.size()) || !output.flush()) {
                    std::error_code currentFileError;
                    std::fs::remove(root / relativePath, currentFileError);
                    for (const auto &prepared : preparedFiles) {
                        std::error_code error;
                        std::fs::remove(root / prepared.relativePath, error);
                    }
                    result.error = fmt::format("hex.builtin.popup.error.project.import_legacy.write_file_failed"_lang, relativePath.generic_string());
                    return result;
                }

                preparedFiles.push_back({ importedProvider.id, file.typeId, std::move(relativePath) });
            }
        }

        for (const auto &file : projectFiles) {
            auto relativePath = std::fs::path(file.name).filename();
            if (relativePath.empty())
                continue;
            const auto stem = relativePath.stem().string();
            const auto extension = relativePath.extension().string();
            for (u32 suffix = 2; ; suffix += 1) {
                std::error_code statusError;
                const auto status = std::fs::symlink_status(root / relativePath, statusError);
                if (statusError && statusError != std::errc::no_such_file_or_directory) {
                    result.error = fmt::format("hex.builtin.popup.error.project.import_legacy.inspect_path_failed"_lang,
                        relativePath.generic_string(), statusError.message());
                    return result;
                }
                if (status.type() == std::fs::file_type::not_found && !reservedPaths.contains(relativePath))
                    break;
                relativePath = std::fs::path(fmt::format("{}-{}{}", stem, suffix, extension));
            }
            reservedPaths.insert(relativePath);

            wolv::io::File output(root / relativePath, wolv::io::File::Mode::Create);
            if (!output.isValid() || output.writeVector(file.contents) != static_cast<i64>(file.contents.size()) || !output.flush()) {
                std::error_code currentFileError;
                std::fs::remove(root / relativePath, currentFileError);
                for (const auto &prepared : preparedFiles) {
                    std::error_code error;
                    std::fs::remove(root / prepared.relativePath, error);
                }
                for (const auto &prepared : preparedProjectFiles) {
                    std::error_code error;
                    std::fs::remove(root / prepared, error);
                }
                result.error = fmt::format("hex.builtin.popup.error.project.import_legacy.write_file_failed"_lang, relativePath.generic_string());
                return result;
            }
            preparedProjectFiles.push_back(std::move(relativePath));
        }

        result.importedProviderCount = providers.size();
        result.importedFileCount = preparedFiles.size() + preparedProjectFiles.size();
        std::ranges::stable_sort(providers, [](const auto &left, const auto &right) {
            return left.descriptor["type"] != "hex.builtin.provider.view" &&
                right.descriptor["type"] == "hex.builtin.provider.view";
        });
        for (auto &importedProvider : providers) {
            const auto id = importedProvider.id;
            auto providerName = getProviderName(importedProvider.descriptor);
            s_projectProviderIds.insert(id);
            s_projectProviderSettings[id] = importedProvider.descriptor.dump(4);

            std::ignore = openStoredProjectProvider(id);
            auto *provider = getProviderById(id);
            if (provider == nullptr) {
                result.failedProviderIds.push_back(id);
            } else {
                providerName = provider->getName();
            }

            for (const auto &file : preparedFiles) {
                if (file.providerId != id)
                    continue;
                s_associations[id][file.typeId] = file.relativePath;
                if (provider != nullptr && FileBackedProviderDataRegistry::get(file.typeId) != nullptr &&
                    !FileBackedProviderDataRegistry::bind(provider, file.typeId, root / file.relativePath)) {
                    log::warn("Failed to bind imported project file '{}' to provider {}", file.relativePath.string(), id);
                }
            }

            if (provider != nullptr && provider->isWritable() && !importedProvider.patches.empty()) {
                size_t appliedPatches = 0;
                for (const auto &[address, value] : importedProvider.patches)
                    if (provider->getRegionValidity(address).second) {
                        provider->write(address, &value, sizeof(value));
                        appliedPatches += 1;
                    }
                if (appliedPatches > 0)
                    provider->getUndoStack().groupOperations(appliedPatches, "hex.builtin.undo_operation.patches");
                if (appliedPatches != importedProvider.patches.size())
                    result.warnings.push_back(fmt::format("hex.builtin.popup.project.import_legacy.warning.patches_not_applied"_lang, providerName));
            } else if (!importedProvider.patches.empty()) {
                result.warnings.push_back(fmt::format("hex.builtin.popup.project.import_legacy.warning.patches_not_imported"_lang, providerName));
            }
        }

        result.success = persistProjectMetadata();
        if (!result.success)
            result.error = "hex.builtin.popup.error.project.import_legacy.save_metadata_failed"_lang.get();
        return result;
    }

    void registerProjectHandlers() {
        hex::ProjectManager::setProjectFunctions(load, store);
        ContentRegistry::UserInterface::addSidebarItem("hex.builtin.sidebar.project.name", ICON_VS_NOTEBOOK, drawProjectSidebar, [] {
            return ProjectManager::isFolderProject();
        });
        EventFileBackedProviderDataChanged::subscribe([](prv::Provider *, FileBackedProviderDataBase *) {
            if (!ProjectManager::isFolderProject())
                return;

            materializeRegisteredData();
        });
        EventProviderOpened::subscribe([](prv::Provider *provider) {
            const auto replacement = s_providerReplacements.find(provider);
            const bool isReplacement = replacement != s_providerReplacements.end();
            if (isReplacement) {
                provider->setID(replacement->second);
                s_providerReplacements.erase(replacement);
            }
            if (ProjectManager::isFolderProject() && !s_loadingProject) {
                s_closedProjectProviderIds.erase(provider->getID());
                snapshotProviderSettings(provider);
                if (isReplacement)
                    bindRegisteredData();
            }
            scheduleProjectMetadataSave();
        });
        EventProviderRemoving::subscribe([](prv::Provider *provider) {
            if (s_loadingProject || !ProjectManager::isFolderProject() ||
                !s_projectProviderIds.contains(provider->getID()) || s_providerOpenAttempts.contains(provider->getID()) ||
                isProviderReplacement(provider))
                return;

            snapshotProviderSettings(provider);
        });
        EventProviderClosed::subscribe([](prv::Provider *provider) {
            if (isProviderReplacement(provider)) {
                s_providerReplacements.erase(provider);
                return;
            }
            if (s_loadingProject || !ProjectManager::isFolderProject() ||
                !s_projectProviderIds.contains(provider->getID()) || s_providerOpenAttempts.contains(provider->getID()))
                return;

            s_closedProjectProviderIds.insert(provider->getID());
            scheduleProjectMetadataSave();
        });
        EventProjectClosed::subscribe([] {
            s_associations.clear();
            s_projectProviderIds.clear();
            s_closedProjectProviderIds.clear();
            s_providerOpenAttempts.clear();
            s_providerReplacements.clear();
            s_projectProviderSettings.clear();
            s_inlineEdit.reset();
        });
        EventProviderDirtied::subscribe([](prv::Provider *) {
            scheduleProjectMetadataSave();
        });
    }
}
