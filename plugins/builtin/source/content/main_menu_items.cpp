#include <hex/api/content_registry.hpp>

#include <imgui.h>
#include <implot.h>

#include <hex/ui/view.hpp>
#include <hex/api/shortcut_manager.hpp>
#include <hex/api/project_file_manager.hpp>
#include <hex/api/layout_manager.hpp>
#include <hex/api/achievement_manager.hpp>
#include <hex/api/events/requests_gui.hpp>
#include <hex/api/tutorial_manager.hpp>

#include <hex/helpers/crypto.hpp>
#include <hex/helpers/patches.hpp>
#include <hex/helpers/debugging.hpp>

#include <content/global_actions.hpp>
#include <toasts/toast_notification.hpp>
#include <popups/popup_text_input.hpp>
#include <hex/api/workspace_manager.hpp>
#include <hex/api/events/events_interaction.hpp>

#include <wolv/io/file.hpp>
#include <wolv/literals.hpp>

#include <romfs/romfs.hpp>
#include <hex/helpers/menu_items.hpp>

#include <GLFW/glfw3.h>

using namespace std::literals::string_literals;
using namespace wolv::literals;

namespace hex::plugin::builtin {

    namespace {

        bool noRunningTasks() {
            return TaskManager::getRunningTaskCount() == 0;
        }

        bool noRunningTaskAndValidProvider() {
            return noRunningTasks() && ImHexApi::Provider::isValid();
        }

        bool noRunningTaskAndWritableProvider() {
            return noRunningTasks() && ImHexApi::Provider::isValid() && ImHexApi::Provider::get()->isWritable();
        }

    }

    namespace {

        void handleIPSError(IPSError error) {
            TaskManager::doLater([error]{
                switch (error) {
                    case IPSError::InvalidPatchHeader:
                        ui::ToastError::open("hex.builtin.menu.file.export.ips.popup.invalid_patch_header_error"_lang);
                        break;
                    case IPSError::AddressOutOfRange:
                        ui::ToastError::open("hex.builtin.menu.file.export.ips.popup.address_out_of_range_error"_lang);
                        break;
                    case IPSError::PatchTooLarge:
                        ui::ToastError::open("hex.builtin.menu.file.export.ips.popup.patch_too_large_error"_lang);
                        break;
                    case IPSError::InvalidPatchFormat:
                        ui::ToastError::open("hex.builtin.menu.file.export.ips.popup.invalid_patch_format_error"_lang);
                        break;
                    case IPSError::MissingEOF:
                        ui::ToastError::open("hex.builtin.menu.file.export.ips.popup.missing_eof_error"_lang);
                        break;
                }
            });
        }

    }

    // Import
    namespace {

        void importIPSPatch() {
            fs::openFileBrowser(fs::DialogMode::Open, {}, [](const auto &path) {
                TaskManager::createTask("hex.ui.common.processing", TaskManager::NoProgress, [path](auto &task) {
                    auto patchData = wolv::io::File(path, wolv::io::File::Mode::Read).readVector();
                    auto patch = Patches::fromIPSPatch(patchData);
                    if (!patch.has_value()) {
                        handleIPSError(patch.error());
                        return;
                    }

                    task.setMaxValue(patch->get().size());

                    auto provider = ImHexApi::Provider::get();

                    for (auto &[address, value] : patch->get()) {
                        provider->write(address, &value, sizeof(value));
                        task.increment();
                    }

                    provider->getUndoStack().groupOperations(patch->get().size(), "hex.builtin.undo_operation.patches");
                });
            });
        }

        void importIPS32Patch() {
            fs::openFileBrowser(fs::DialogMode::Open, {}, [](const auto &path) {
                TaskManager::createTask("hex.ui.common.processing", TaskManager::NoProgress, [path](auto &task) {
                    auto patchData = wolv::io::File(path, wolv::io::File::Mode::Read).readVector();
                    auto patch = Patches::fromIPS32Patch(patchData);
                    if (!patch.has_value()) {
                        handleIPSError(patch.error());
                        return;
                    }

                    task.setMaxValue(patch->get().size());

                    auto provider = ImHexApi::Provider::get();

                    for (auto &[address, value] : patch->get()) {
                        provider->write(address, &value, sizeof(value));
                        task.increment();
                    }

                    provider->getUndoStack().groupOperations(patch->get().size(), "hex.builtin.undo_operation.patches");
                });
            });
        }

        void importModifiedFile() {
            fs::openFileBrowser(fs::DialogMode::Open, {}, [](const auto &path) {
                TaskManager::createTask("hex.ui.common.processing", TaskManager::NoProgress, [path](auto &task) {
                    auto provider = ImHexApi::Provider::get();
                    auto patchData = wolv::io::File(path, wolv::io::File::Mode::Read).readVector();

                    if (patchData.size() != provider->getActualSize()) {
                        ui::ToastError::open("hex.builtin.menu.file.import.modified_file.popup.invalid_size"_lang);
                        return;
                    }

                    const auto baseAddress = provider->getBaseAddress();

                    std::map<u64, u8> patches;
                    for (u64 i = 0; i < patchData.size(); i++) {
                        u8 value = 0;
                        provider->read(baseAddress + i, &value, 1);

                        if (value != patchData[i])
                            patches[baseAddress + i] = patchData[i];
                    }

                    task.setMaxValue(patches.size());

                    for (auto &[address, value] : patches) {
                        provider->write(address, &value, sizeof(value));
                        task.increment();
                    }

                    provider->getUndoStack().groupOperations(patches.size(), "hex.builtin.undo_operation.patches");
                });
            });
        }

    }

    // Export
    namespace {

        void exportBase64() {
            fs::openFileBrowser(fs::DialogMode::Save, {}, [](const auto &path) {
                TaskManager::createTask("hex.ui.common.processing", TaskManager::NoProgress, [path](auto &) {
                    wolv::io::File outputFile(path, wolv::io::File::Mode::Create);
                    if (!outputFile.isValid()) {
                        TaskManager::doLater([] {
                            ui::ToastError::open("hex.builtin.menu.file.export.error.create_file"_lang);
                        });
                        return;
                    }

                    auto provider = ImHexApi::Provider::get();
                    std::vector<u8> bytes(5_MiB);
                    for (u64 address = 0; address < provider->getActualSize(); address += bytes.size()) {
                        bytes.resize(std::min<u64>(bytes.size(), provider->getActualSize() - address));
                        provider->read(provider->getBaseAddress() + address, bytes.data(), bytes.size());

                        outputFile.writeVector(crypt::encode64(bytes));
                    }
                });
            });
        }

        void exportSelectionToFile() {
            fs::openFileBrowser(fs::DialogMode::Save, {}, [](const auto &path) {
                TaskManager::createTask("hex.ui.common.processing", TaskManager::NoProgress, [path](auto &task) {
                    wolv::io::File outputFile(path, wolv::io::File::Mode::Create);
                    if (!outputFile.isValid()) {
                        TaskManager::doLater([] {
                            ui::ToastError::open("hex.builtin.menu.file.export.error.create_file"_lang);
                        });
                        return;
                    }

                    auto provider = ImHexApi::Provider::get();
                    std::vector<u8> bytes(5_MiB);

                    auto selection = ImHexApi::HexEditor::getSelection();
                    for (u64 address = selection->getStartAddress(); address <= selection->getEndAddress(); address += bytes.size()) {
                        bytes.resize(std::min<u64>(bytes.size(), selection->getEndAddress() - address + 1));
                        provider->read(address, bytes.data(), bytes.size());

                        outputFile.writeVector(bytes);
                        task.update();
                    }
                });
            });
        }

        void drawExportLanguageMenu() {
            for (const auto &formatter : ContentRegistry::DataFormatter::impl::getExportMenuEntries()) {
                if (menu::menuItem(Lang(formatter.unlocalizedName), Shortcut::None, false, ImHexApi::Provider::isValid() && ImHexApi::Provider::get()->getActualSize() > 0)) {
                    fs::openFileBrowser(fs::DialogMode::Save, {}, [&formatter](const auto &path) {
                        TaskManager::createTask("hex.builtin.task.exporting_data", TaskManager::NoProgress, [&formatter, path](auto&){
                            auto provider = ImHexApi::Provider::get();
                            auto selection = ImHexApi::HexEditor::getSelection()
                                    .value_or(
                                            ImHexApi::HexEditor::ProviderRegion {
                                                { provider->getBaseAddress(), provider->getSize() },
                                                provider
                                            });

                            auto result = formatter.callback(provider, selection.getStartAddress(), selection.getSize(), false);

                            wolv::io::File file(path, wolv::io::File::Mode::Create);
                            if (!file.isValid()) {
                                TaskManager::doLater([] {
                                    ui::ToastError::open("hex.builtin.menu.file.export.as_language.popup.export_error"_lang);
                                });
                                return;
                            }

                            file.writeString(result);
                        });
                    });
                }
            }
        }

        void exportReport() {
            TaskManager::createTask("hex.ui.common.processing", TaskManager::NoProgress, [](auto &) {
                std::string data;

                for (const auto &provider : ImHexApi::Provider::getProviders()) {
                    data += fmt::format("# {}", provider->getName());
                    data += "\n\n";

                    for (const auto &generator : ContentRegistry::Reports::impl::getGenerators()) {
                        data += generator.callback(provider);
                        data += "\n\n";
                    }
                    data += "\n\n";
                }

                TaskManager::doLater([data] {
                    fs::openFileBrowser(fs::DialogMode::Save, { { "Markdown File", "md" }}, [&data](const auto &path) {
                        auto file = wolv::io::File(path, wolv::io::File::Mode::Create);
                        if (!file.isValid()) {
                            ui::ToastError::open("hex.builtin.menu.file.export.report.popup.export_error"_lang);
                            return;
                        }

                        file.writeString(data);
                    });
                });
            });
        }

        void exportIPSPatch() {
            auto provider = ImHexApi::Provider::get();

            auto patches = Patches::fromProvider(provider);
            if (!patches.has_value()) {
                handleIPSError(patches.error());
                return;
            }

            // Make sure there's no patch at address 0x00454F46 because that would cause the patch to contain the sequence "EOF" which signals the end of the patch
            if (!patches->get().contains(0x00454F45) && patches->get().contains(0x00454F46)) {
                u8 value = 0;
                provider->read(0x00454F45, &value, sizeof(u8));
                patches->get().at(0x00454F45) = value;
            }

            TaskManager::createTask("hex.ui.common.processing", TaskManager::NoProgress, [patches](auto &) {
                auto data = patches->toIPSPatch();

                TaskManager::doLater([data] {
                    fs::openFileBrowser(fs::DialogMode::Save, {}, [&data](const auto &path) {
                        auto file = wolv::io::File(path, wolv::io::File::Mode::Create);
                        if (!file.isValid()) {
                            ui::ToastError::open("hex.builtin.menu.file.export.ips.popup.export_error"_lang);
                            return;
                        }

                        if (data.has_value()) {
                            const auto& bytes = data.value();
                            file.writeVector(bytes);
                            EventPatchCreated::post(bytes.data(), bytes.size(), PatchKind::IPS);
                        } else {
                            handleIPSError(data.error());
                        }
                    });
                });
            });
        }

        void exportIPS32Patch() {
            auto provider = ImHexApi::Provider::get();

            auto patches = Patches::fromProvider(provider);
            if (!patches.has_value()) {
                handleIPSError(patches.error());
                return;
            }

            // Make sure there's no patch at address 0x45454F46 because that would cause the patch to contain the sequence "*EOF" which signals the end of the patch
            if (!patches->get().contains(0x45454F45) && patches->get().contains(0x45454F46)) {
                u8 value = 0;
                provider->read(0x45454F45, &value, sizeof(u8));
                patches->get().at(0x45454F45) = value;
            }

            TaskManager::createTask("hex.ui.common.processing", TaskManager::NoProgress, [patches](auto &) {
                auto data = patches->toIPS32Patch();

                TaskManager::doLater([data] {
                    fs::openFileBrowser(fs::DialogMode::Save, {}, [&data](const auto &path) {
                        auto file = wolv::io::File(path, wolv::io::File::Mode::Create);
                        if (!file.isValid()) {
                            ui::ToastError::open("hex.builtin.menu.file.export.ips.popup.export_error"_lang);
                            return;
                        }

                        if (data.has_value()) {
                            const std::vector<u8>& bytes = data.value();
                            file.writeVector(bytes);
                            EventPatchCreated::post(bytes.data(), bytes.size(), PatchKind::IPS32);
                        } else {
                            handleIPSError(data.error());
                        }
                    });
                });
            });
        }

    }


    /**
     * @brief returns true if there is a currently selected provider, and it is possibl to dump data from it
     */
    bool isProviderDumpable() {
        auto provider = ImHexApi::Provider::get();
        return ImHexApi::Provider::isValid() && provider->isDumpable();
    }

    static void createFileMenu() {

        ContentRegistry::Interface::registerMainMenuItem("hex.builtin.menu.file", 1000);

        /* Create File */
        ContentRegistry::Interface::addMenuItem({ "hex.builtin.menu.file", "hex.builtin.menu.file.create_file" }, ICON_VS_FILE, 1050, CTRLCMD + Keys::N + AllowWhileTyping, [] {
            auto newProvider = hex::ImHexApi::Provider::createProvider("hex.builtin.provider.mem_file", true);
            if (newProvider != nullptr && !newProvider->open())
                hex::ImHexApi::Provider::remove(newProvider);
            else
                EventProviderOpened::post(newProvider);
        }, noRunningTasks);

        /* Open File */
        ContentRegistry::Interface::addMenuItem({ "hex.builtin.menu.file", "hex.builtin.menu.file.open_file" }, ICON_VS_FOLDER_OPENED, 1100, CTRLCMD + Keys::O + AllowWhileTyping, [] {
            RequestOpenWindow::post("Open File");
        }, noRunningTasks, ContentRegistry::Views::getViewByName("hex.builtin.view.hex_editor.name"));

        /* Open Other */
        ContentRegistry::Interface::addMenuItemSubMenu({ "hex.builtin.menu.file", "hex.builtin.menu.file.open_other"}, ICON_VS_TELESCOPE, 1150, [] {
            for (const auto &[unlocalizedProviderName, icon] : ContentRegistry::Provider::impl::getEntries()) {
                if (menu::menuItemEx(Lang(unlocalizedProviderName), icon))
                    ImHexApi::Provider::createProvider(unlocalizedProviderName);
            }
        }, noRunningTasks);

        /* Reload Provider */
        ContentRegistry::Interface::addMenuItem({ "hex.builtin.menu.file", "hex.builtin.menu.file.reload_provider"}, ICON_VS_REFRESH, 1250, CTRLCMD + Keys::R + AllowWhileTyping, [] {
            auto provider = ImHexApi::Provider::get();

            provider->close();
            if (!provider->open())
                ImHexApi::Provider::remove(provider, true);

            EventDataChanged::post(provider);
        }, noRunningTaskAndValidProvider);


        /* Project open / save */
        ContentRegistry::Interface::addMenuItemSubMenu({ "hex.builtin.menu.file", "hex.builtin.menu.file.project" }, ICON_VS_NOTEBOOK, 1400, []{}, noRunningTasks);

        ContentRegistry::Interface::addMenuItem({ "hex.builtin.menu.file", "hex.builtin.menu.file.project", "hex.builtin.menu.file.project.open" }, ICON_VS_ROOT_FOLDER_OPENED, 1410,
                                                CTRL + ALT + Keys::O + AllowWhileTyping,
                                                openProject, noRunningTasks);

        ContentRegistry::Interface::addMenuItem({ "hex.builtin.menu.file", "hex.builtin.menu.file.project", "hex.builtin.menu.file.project.save" }, ICON_VS_SAVE, 1450,
                                                CTRL + ALT + Keys::S + AllowWhileTyping,
                                                saveProject, [&] { return noRunningTaskAndValidProvider() && ProjectFile::hasPath(); });

        ContentRegistry::Interface::addMenuItem({ "hex.builtin.menu.file", "hex.builtin.menu.file.project", "hex.builtin.menu.file.project.save_as" }, ICON_VS_SAVE_AS, 1500,
                                                ALT + SHIFT + Keys::S + AllowWhileTyping,
                                                saveProjectAs, noRunningTaskAndValidProvider);


        ContentRegistry::Interface::addMenuItemSeparator({ "hex.builtin.menu.file" }, 2000);

        /* Import */
        {
            ContentRegistry::Interface::addMenuItemSubMenu({ "hex.builtin.menu.file", "hex.builtin.menu.file.import" }, ICON_VS_SIGN_IN, 5140, []{}, noRunningTaskAndValidProvider);

            /* IPS */
            ContentRegistry::Interface::addMenuItem({ "hex.builtin.menu.file", "hex.builtin.menu.file.import", "hex.builtin.menu.file.import.ips"}, ICON_VS_GIT_PULL_REQUEST_NEW_CHANGES, 5150,
                                                    Shortcut::None,
                                                    importIPSPatch,
                                                    noRunningTaskAndWritableProvider);

            /* IPS32 */
            ContentRegistry::Interface::addMenuItem({ "hex.builtin.menu.file", "hex.builtin.menu.file.import", "hex.builtin.menu.file.import.ips32"}, ICON_VS_GIT_PULL_REQUEST_NEW_CHANGES, 5200,
                                                    Shortcut::None,
                                                    importIPS32Patch,
                                                    noRunningTaskAndWritableProvider);

            /* Modified File */
            ContentRegistry::Interface::addMenuItem({ "hex.builtin.menu.file", "hex.builtin.menu.file.import", "hex.builtin.menu.file.import.modified_file" }, ICON_VS_FILES, 5300,
                                                    Shortcut::None,
                                                    importModifiedFile,
                                                    noRunningTaskAndWritableProvider);
        }

        /* Export */
        /* Only make them accessible if the current provider is dumpable */
        {
            ContentRegistry::Interface::addMenuItemSubMenu({ "hex.builtin.menu.file", "hex.builtin.menu.file.export" }, ICON_VS_SIGN_OUT, 6000, []{}, isProviderDumpable);

            /* Selection to File */
            ContentRegistry::Interface::addMenuItem({ "hex.builtin.menu.file", "hex.builtin.menu.file.export", "hex.builtin.menu.file.export.selection_to_file" }, ICON_VS_FILE_BINARY, 6010,
                                                    Shortcut::None,
                                                    exportSelectionToFile,
                                                    ImHexApi::HexEditor::isSelectionValid);

            /* Base 64 */
            ContentRegistry::Interface::addMenuItem({ "hex.builtin.menu.file", "hex.builtin.menu.file.export", "hex.builtin.menu.file.export.base64" }, ICON_VS_NOTE, 6020,
                                                    Shortcut::None,
                                                    exportBase64,
                                                    isProviderDumpable);

            /* Language */
            ContentRegistry::Interface::addMenuItemSubMenu({ "hex.builtin.menu.file", "hex.builtin.menu.file.export", "hex.builtin.menu.file.export.as_language" }, ICON_VS_CODE, 6030,
                                                    drawExportLanguageMenu,
                                                    isProviderDumpable);

            /* Report */
            ContentRegistry::Interface::addMenuItem({ "hex.builtin.menu.file", "hex.builtin.menu.file.export", "hex.builtin.menu.file.export.report" }, ICON_VS_MARKDOWN, 6040,
                                                    Shortcut::None,
                                                    exportReport,
                                                    ImHexApi::Provider::isValid);

            ContentRegistry::Interface::addMenuItemSeparator({ "hex.builtin.menu.file", "hex.builtin.menu.file.export" }, 6050);

            /* IPS */
            ContentRegistry::Interface::addMenuItem({ "hex.builtin.menu.file", "hex.builtin.menu.file.export", "hex.builtin.menu.file.export.ips" }, ICON_VS_GIT_PULL_REQUEST_NEW_CHANGES, 6100,
                                                    Shortcut::None,
                                                    exportIPSPatch,
                                                    isProviderDumpable);

            /* IPS32 */
            ContentRegistry::Interface::addMenuItem({ "hex.builtin.menu.file", "hex.builtin.menu.file.export", "hex.builtin.menu.file.export.ips32" }, ICON_VS_GIT_PULL_REQUEST_NEW_CHANGES, 6150,
                                                    Shortcut::None,
                                                    exportIPS32Patch,
                                                    isProviderDumpable);
        }

        ContentRegistry::Interface::addMenuItemSeparator({ "hex.builtin.menu.file" }, 10000);

        /* Close Provider */
        ContentRegistry::Interface::addMenuItem({ "hex.builtin.menu.file", "hex.builtin.menu.file.close"}, ICON_VS_CHROME_CLOSE, 10050, CTRLCMD + Keys::W + AllowWhileTyping, [] {
            ImHexApi::Provider::remove(ImHexApi::Provider::get());
        }, noRunningTaskAndValidProvider);

        /* Quit ImHex */
        ContentRegistry::Interface::addMenuItem({ "hex.builtin.menu.file", "hex.builtin.menu.file.quit"}, ICON_VS_CLOSE_ALL, 10100, ALT + Keys::F4 + AllowWhileTyping, [] {
            ImHexApi::System::closeImHex();
        });
    }

    static void createEditMenu() {
        ContentRegistry::Interface::registerMainMenuItem("hex.builtin.menu.edit", 2000);
    }

    static void createViewMenu() {
        ContentRegistry::Interface::registerMainMenuItem("hex.builtin.menu.view", 3000);

        #if !defined(OS_WEB)
            ContentRegistry::Interface::addMenuItem({ "hex.builtin.menu.view", "hex.builtin.menu.view.always_on_top" }, ICON_VS_PINNED, 1000, Keys::F10 + AllowWhileTyping, [] {
                static bool state = false;

                state = !state;
                glfwSetWindowAttrib(ImHexApi::System::getMainWindowHandle(), GLFW_FLOATING, state);
            }, []{ return true; }, []{ return glfwGetWindowAttrib(ImHexApi::System::getMainWindowHandle(), GLFW_FLOATING); });
        #endif

        #if !defined(OS_MACOS) && !defined(OS_WEB)
            ContentRegistry::Interface::addMenuItem({ "hex.builtin.menu.view", "hex.builtin.menu.view.fullscreen" }, ICON_VS_SCREEN_FULL, 2000, Keys::F11 + AllowWhileTyping, [] {
                static bool state = false;
                static ImVec2 position, size;

                state = !state;


                const auto window = ImHexApi::System::getMainWindowHandle();
                if (state) {
                    position = ImHexApi::System::getMainWindowPosition();
                    size     = ImHexApi::System::getMainWindowSize();

                    const auto monitor = glfwGetPrimaryMonitor();
                    const auto videoMode = glfwGetVideoMode(monitor);

                    glfwSetWindowMonitor(window, monitor, 0, 0, videoMode->width, videoMode->height, videoMode->refreshRate);
                } else {
                    glfwSetWindowMonitor(window, nullptr, position.x, position.y, size.x, size.y, 0);
                    glfwSetWindowAttrib(window, GLFW_DECORATED, ImHexApi::System::isBorderlessWindowModeEnabled() ? GLFW_FALSE : GLFW_TRUE);
                }

            }, []{ return true; }, []{ return glfwGetWindowMonitor(ImHexApi::System::getMainWindowHandle()) != nullptr; });
        #endif

        #if !defined(OS_WEB)
            ContentRegistry::Interface::addMenuItemSeparator({ "hex.builtin.menu.view" }, 3000);
        #endif

        ContentRegistry::Interface::addMenuItemSubMenu({ "hex.builtin.menu.view" }, 4000, [] {
            for (auto &[name, view] : ContentRegistry::Views::impl::getEntries()) {
                if (view->hasViewMenuItemEntry()) {
                    auto &state = view->getWindowOpenState();

                    if (menu::menuItemEx(Lang(view->getUnlocalizedName()), view->getIcon(), Shortcut::None, state, ImHexApi::Provider::isValid() && !LayoutManager::isLayoutLocked()))
                        state = !state;
                }
            }
        });

    }

    static void createLayoutMenu() {
        LayoutManager::reload();

        ContentRegistry::Interface::addMenuItemSubMenu({ "hex.builtin.menu.workspace", "hex.builtin.menu.workspace.layout" }, ICON_VS_LAYOUT, 1050, []{}, ImHexApi::Provider::isValid);

        ContentRegistry::Interface::addMenuItem({ "hex.builtin.menu.workspace", "hex.builtin.menu.workspace.layout", "hex.builtin.menu.workspace.layout.save" }, 1100, Shortcut::None, [] {
            ui::PopupTextInput::open("hex.builtin.popup.save_layout.title", "hex.builtin.popup.save_layout.desc", [](const std::string &name) {
                LayoutManager::save(name);
            });
        }, ImHexApi::Provider::isValid);

        ContentRegistry::Interface::addMenuItemSubMenu({ "hex.builtin.menu.workspace", "hex.builtin.menu.workspace.layout" }, ICON_VS_LAYOUT, 1150, [] {
            bool locked = LayoutManager::isLayoutLocked();
            if (menu::menuItemEx("hex.builtin.menu.workspace.layout.lock"_lang, ICON_VS_LOCK, Shortcut::None, locked, ImHexApi::Provider::isValid())) {
                LayoutManager::lockLayout(!locked);
                ContentRegistry::Settings::write<bool>("hex.builtin.setting.interface", "hex.builtin.setting.interface.layout_locked", !locked);
            }
        });

        ContentRegistry::Interface::addMenuItemSeparator({ "hex.builtin.menu.workspace", "hex.builtin.menu.workspace.layout" }, 1200);

        ContentRegistry::Interface::addMenuItemSubMenu({ "hex.builtin.menu.workspace", "hex.builtin.menu.workspace.layout" }, 2000, [] {
            for (const auto &path : romfs::list("layouts")) {
                if (menu::menuItem(wolv::util::capitalizeString(path.stem().string()).c_str(), Shortcut::None, false, ImHexApi::Provider::isValid())) {
                    LayoutManager::loadFromString(std::string(romfs::get(path).string()));
                }
            }

            bool shiftPressed = ImGui::GetIO().KeyShift;
            for (auto &[name, path] : LayoutManager::getLayouts()) {
                if (menu::menuItem(fmt::format("{}{}", name, shiftPressed ? " " ICON_VS_CHROME_CLOSE : "").c_str(), Shortcut::None, false, ImHexApi::Provider::isValid())) {
                    if (shiftPressed) {
                        LayoutManager::removeLayout(name);
                        break;
                    } else {
                        LayoutManager::load(path);
                    }
                }
            }
        });
    }

    static void createWorkspaceMenu() {
        ContentRegistry::Interface::registerMainMenuItem("hex.builtin.menu.workspace", 4000);

        createLayoutMenu();

        ContentRegistry::Interface::addMenuItemSeparator({ "hex.builtin.menu.workspace" }, 3000);

        ContentRegistry::Interface::addMenuItem({ "hex.builtin.menu.workspace", "hex.builtin.menu.workspace.create" }, ICON_VS_ADD, 3100, Shortcut::None, [] {
            ui::PopupTextInput::open("hex.builtin.popup.create_workspace.title", "hex.builtin.popup.create_workspace.desc", [](const std::string &name) {
                WorkspaceManager::createWorkspace(name);
            });
        }, ImHexApi::Provider::isValid);

        ContentRegistry::Interface::addMenuItemSubMenu({ "hex.builtin.menu.workspace" }, 3200, [] {
            const auto &workspaces = WorkspaceManager::getWorkspaces();

            bool shiftPressed = ImGui::GetIO().KeyShift;
            for (auto it = workspaces.begin(); it != workspaces.end(); ++it) {
                const auto &[name, workspace] = *it;

                bool canRemove = shiftPressed && !workspace.builtin;
                if (menu::menuItem(fmt::format("{}{}", name, canRemove ? " " ICON_VS_CHROME_CLOSE : "").c_str(), Shortcut::None, it == WorkspaceManager::getCurrentWorkspace(), ImHexApi::Provider::isValid())) {
                    if (canRemove) {
                        WorkspaceManager::removeWorkspace(name);
                        break;
                    } else {
                        WorkspaceManager::switchWorkspace(name);
                    }
                }
            }
        });
    }

    static void createExtrasMenu() {
        ContentRegistry::Interface::registerMainMenuItem("hex.builtin.menu.extras", 5000);

        if (ImHexApi::System::isNightlyBuild()) {
            ContentRegistry::Interface::addMenuItem({ "hex.builtin.menu.extras", "hex.builtin.menu.extras.switch_to_stable" }, ICON_VS_ROCKET, 2750, Shortcut::None, [] {
                ImHexApi::System::updateImHex(ImHexApi::System::UpdateType::Stable);
            });
        } else {
            ContentRegistry::Interface::addMenuItem({ "hex.builtin.menu.extras", "hex.builtin.menu.extras.switch_to_nightly" }, ICON_VS_ROCKET, 2750, Shortcut::None, [] {
                ImHexApi::System::updateImHex(ImHexApi::System::UpdateType::Nightly);
            });
        }
    }

    static void createHelpMenu() {
        ContentRegistry::Interface::registerMainMenuItem("hex.builtin.menu.help", 6000);
    }


    void registerMainMenuEntries() {
        createFileMenu();
        createEditMenu();
        createViewMenu();
        createWorkspaceMenu();
        createExtrasMenu();
        createHelpMenu();
    }

}