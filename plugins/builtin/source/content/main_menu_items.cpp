#include <hex/api/content_registry.hpp>

#include <imgui.h>
#include <implot.h>

#include <hex/ui/view.hpp>
#include <hex/api/keybinding.hpp>
#include <hex/api/project_file_manager.hpp>

#include <hex/helpers/crypto.hpp>
#include <hex/helpers/patches.hpp>

#include "content/global_actions.hpp"
#include <content/popups/popup_notification.hpp>
#include <content/popups/popup_docs_question.hpp>

#include <wolv/io/file.hpp>

using namespace std::literals::string_literals;

namespace hex::plugin::builtin {

    static bool g_demoWindowOpen = false;

    namespace {

        bool noRunningTasks() {
            return TaskManager::getRunningTaskCount() == 0;
        }

        bool noRunningTaskAndValidProvider() {
            return noRunningTasks() && ImHexApi::Provider::isValid();
        }

    }

    namespace {

        void handleIPSError(IPSError error) {
            TaskManager::doLater([error]{
                switch (error) {
                    case IPSError::InvalidPatchHeader:
                        PopupError::open("hex.builtin.menu.file.export.ips.popup.invalid_patch_header_error"_lang);
                        break;
                    case IPSError::AddressOutOfRange:
                        PopupError::open("hex.builtin.menu.file.export.ips.popup.address_out_of_range_error"_lang);
                        break;
                    case IPSError::PatchTooLarge:
                        PopupError::open("hex.builtin.menu.file.export.ips.popup.patch_too_large_error"_lang);
                        break;
                    case IPSError::InvalidPatchFormat:
                        PopupError::open("hex.builtin.menu.file.export.ips.popup.invalid_patch_format_error"_lang);
                        break;
                    case IPSError::MissingEOF:
                        PopupError::open("hex.builtin.menu.file.export.ips.popup.missing_eof_error"_lang);
                        break;
                }
            });
        }

    }

    // Import
    namespace {

        void importBase64() {
            fs::openFileBrowser(fs::DialogMode::Open, {}, [](const auto &path) {
                wolv::io::File inputFile(path, wolv::io::File::Mode::Read);
                if (!inputFile.isValid()) {
                    PopupError::open("hex.builtin.menu.file.import.base64.popup.open_error"_lang);
                    return;
                }

                auto base64 = inputFile.readVector();

                if (!base64.empty()) {
                    auto data = crypt::decode64(base64);

                    if (data.empty())
                        PopupError::open("hex.builtin.menu.file.import.base64.popup.import_error"_lang);
                    else {
                        fs::openFileBrowser(fs::DialogMode::Save, {}, [&data](const std::fs::path &path) {
                            wolv::io::File outputFile(path, wolv::io::File::Mode::Create);

                            if (!outputFile.isValid())
                                PopupError::open("hex.builtin.menu.file.import.base64.popup.import_error"_lang);

                            outputFile.writeVector(data);
                        });
                    }
                } else {
                    PopupError::open("hex.builtin.popup.file_open_error"_lang);
                }
            });
        }

        void importIPSPatch() {
            fs::openFileBrowser(fs::DialogMode::Open, {}, [](const auto &path) {
                TaskManager::createTask("hex.builtin.common.processing", TaskManager::NoProgress, [path](auto &task) {
                    auto patchData = wolv::io::File(path, wolv::io::File::Mode::Read).readVector();
                    auto patch     = hex::loadIPSPatch(patchData);
                    if (!patch.has_value()) {
                        handleIPSError(patch.error());
                        return;
                    }

                    task.setMaxValue(patch->size());

                    auto provider = ImHexApi::Provider::get();

                    u64 progress = 0;
                    for (auto &[address, value] : *patch) {
                        provider->addPatch(address, &value, 1);
                        progress++;
                        task.update(progress);
                    }

                    provider->createUndoPoint();
                });
            });
        }

        void importIPS32Patch() {
            fs::openFileBrowser(fs::DialogMode::Open, {}, [](const auto &path) {
                TaskManager::createTask("hex.builtin.common.processing", TaskManager::NoProgress, [path](auto &task) {
                    auto patchData = wolv::io::File(path, wolv::io::File::Mode::Read).readVector();
                    auto patch = hex::loadIPS32Patch(patchData);
                    if (!patch.has_value()) {
                        handleIPSError(patch.error());
                        return;
                    }

                    task.setMaxValue(patch->size());

                    auto provider = ImHexApi::Provider::get();

                    u64 progress = 0;
                    for (auto &[address, value] : *patch) {
                        provider->addPatch(address, &value, 1);
                        progress++;
                        task.update(progress);
                    }

                    provider->createUndoPoint();
                });
            });
        }

        void importModifiedFile() {
            fs::openFileBrowser(fs::DialogMode::Open, {}, [](const auto &path) {
                TaskManager::createTask("hex.builtin.common.processing", TaskManager::NoProgress, [path](auto &task) {
                    auto provider = ImHexApi::Provider::get();
                    auto patchData = wolv::io::File(path, wolv::io::File::Mode::Read).readVector();

                    if (patchData.size() != provider->getActualSize()) {
                        PopupError::open("hex.builtin.menu.file.import.modified_file.popup.invalid_size"_lang);
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

                    u64 progress = 0;
                    for (auto &[address, value] : patches) {
                        provider->addPatch(address, &value, 1);
                        progress++;
                        task.update(progress);
                    }

                    provider->createUndoPoint();
                });
            });
        }

    }

    // Export
    namespace {

        void exportBase64() {
            fs::openFileBrowser(fs::DialogMode::Save, {}, [](const auto &path) {
                TaskManager::createTask("hex.builtin.common.processing", TaskManager::NoProgress, [path](auto &) {
                    wolv::io::File outputFile(path, wolv::io::File::Mode::Create);
                    if (!outputFile.isValid()) {
                        TaskManager::doLater([] {
                            PopupError::open("hex.builtin.menu.file.export.base64.popup.export_error"_lang);
                        });
                        return;
                    }

                    auto provider = ImHexApi::Provider::get();
                    std::vector<u8> bytes(3000);
                    for (u64 address = 0; address < provider->getActualSize(); address += 3000) {
                        bytes.resize(std::min<u64>(3000, provider->getActualSize() - address));
                        provider->read(provider->getBaseAddress() + address, bytes.data(), bytes.size());

                        outputFile.writeVector(crypt::encode64(bytes));
                    }
                });
            });
        }

        void exportIPSPatch() {
            auto provider = ImHexApi::Provider::get();

            Patches patches = provider->getPatches();

            // Make sure there's no patch at address 0x00454F46 because that would cause the patch to contain the sequence "EOF" which signals the end of the patch
            if (!patches.contains(0x00454F45) && patches.contains(0x00454F46)) {
                u8 value = 0;
                provider->read(0x00454F45, &value, sizeof(u8));
                patches[0x00454F45] = value;
            }

            TaskManager::createTask("hex.builtin.common.processing", TaskManager::NoProgress, [patches](auto &) {
                auto data = generateIPSPatch(patches);

                TaskManager::doLater([data] {
                    fs::openFileBrowser(fs::DialogMode::Save, {}, [&data](const auto &path) {
                        auto file = wolv::io::File(path, wolv::io::File::Mode::Create);
                        if (!file.isValid()) {
                            PopupError::open("hex.builtin.menu.file.export.ips.popup.export_error"_lang);
                            return;
                        }

                        if (data.has_value())
                            file.writeVector(data.value());
                        else {
                            handleIPSError(data.error());
                        }
                    });
                });
            });
        }

        void exportIPS32Patch() {
            auto provider = ImHexApi::Provider::get();

            Patches patches = provider->getPatches();

            // Make sure there's no patch at address 0x45454F46 because that would cause the patch to contain the sequence "*EOF" which signals the end of the patch
            if (!patches.contains(0x45454F45) && patches.contains(0x45454F46)) {
                u8 value = 0;
                provider->read(0x45454F45, &value, sizeof(u8));
                patches[0x45454F45] = value;
            }

            TaskManager::createTask("hex.builtin.common.processing", TaskManager::NoProgress, [patches](auto &) {
                auto data = generateIPS32Patch(patches);

                TaskManager::doLater([data] {
                    fs::openFileBrowser(fs::DialogMode::Save, {}, [&data](const auto &path) {
                        auto file = wolv::io::File(path, wolv::io::File::Mode::Create);
                        if (!file.isValid()) {
                            PopupError::open("hex.builtin.menu.file.export.ips.popup.export_error"_lang);
                            return;
                        }

                        if (data.has_value())
                            file.writeVector(data.value());
                        else
                            handleIPSError(data.error());
                    });
                });
            });
        }

    }



    static void createFileMenu() {

        ContentRegistry::Interface::registerMainMenuItem("hex.builtin.menu.file", 1000);

        /* Create File */
        ContentRegistry::Interface::addMenuItem({ "hex.builtin.menu.file", "hex.builtin.menu.file.create_file" }, 1050, CTRLCMD + Keys::N, [] {
            auto newProvider = hex::ImHexApi::Provider::createProvider("hex.builtin.provider.mem_file", true);
            if (newProvider != nullptr && !newProvider->open())
                hex::ImHexApi::Provider::remove(newProvider);
            else
                EventManager::post<EventProviderOpened>(newProvider);
        }, noRunningTasks);

        /* Open File */
        ContentRegistry::Interface::addMenuItem({ "hex.builtin.menu.file", "hex.builtin.menu.file.open_file" }, 1100, CTRLCMD + Keys::O, [] {
            EventManager::post<RequestOpenWindow>("Open File");
        }, noRunningTasks);

        /* Open Other */
        ContentRegistry::Interface::addMenuItemSubMenu({ "hex.builtin.menu.file", "hex.builtin.menu.file.open_other"}, 1150, [] {
            for (const auto &unlocalizedProviderName : ContentRegistry::Provider::impl::getEntries()) {
                if (ImGui::MenuItem(LangEntry(unlocalizedProviderName)))
                    ImHexApi::Provider::createProvider(unlocalizedProviderName);
            }
        }, noRunningTasks);

        /* Reload Provider */
        ContentRegistry::Interface::addMenuItem({ "hex.builtin.menu.file", "hex.builtin.menu.file.reload_provider"}, 1250, CTRLCMD + Keys::R, [] {
            auto provider = ImHexApi::Provider::get();

            provider->close();
            if (!provider->open())
                ImHexApi::Provider::remove(provider, true);
        }, noRunningTaskAndValidProvider);


        /* Project open / save */
        ContentRegistry::Interface::addMenuItem({ "hex.builtin.menu.file", "hex.builtin.menu.file.project", "hex.builtin.menu.file.project.open" }, 1400,
                                                ALT + Keys::O,
                                                openProject, noRunningTasks);

        ContentRegistry::Interface::addMenuItem({ "hex.builtin.menu.file", "hex.builtin.menu.file.project", "hex.builtin.menu.file.project.save" }, 1450,
                                                ALT + Keys::S,
                                                saveProject, [&] { return noRunningTaskAndValidProvider() && ProjectFile::hasPath(); });

        ContentRegistry::Interface::addMenuItem({ "hex.builtin.menu.file", "hex.builtin.menu.file.project", "hex.builtin.menu.file.project.save_as" }, 1500,
                                                ALT + SHIFT + Keys::S,
                                                saveProjectAs, [&] { return ImHexApi::Provider::isValid() && ImHexApi::Provider::get()->isWritable(); });


        ContentRegistry::Interface::addMenuItemSeparator({ "hex.builtin.menu.file" }, 2000);

        /* Import */
        {
            /* Base 64 */
            ContentRegistry::Interface::addMenuItem({ "hex.builtin.menu.file", "hex.builtin.menu.file.import", "hex.builtin.menu.file.import.base64" }, 2050,
                                                    Shortcut::None,
                                                    importBase64,
                                                    noRunningTasks);

            ContentRegistry::Interface::addMenuItemSeparator({ "hex.builtin.menu.file", "hex.builtin.menu.file.import" }, 2100);

            /* IPS */
            ContentRegistry::Interface::addMenuItem({ "hex.builtin.menu.file", "hex.builtin.menu.file.import", "hex.builtin.menu.file.import.ips"}, 2150,
                                                    Shortcut::None,
                                                    importIPSPatch,
                                                    ImHexApi::Provider::isValid);

            /* IPS32 */
            ContentRegistry::Interface::addMenuItem({ "hex.builtin.menu.file", "hex.builtin.menu.file.import", "hex.builtin.menu.file.import.ips32"}, 2200,
                                                    Shortcut::None,
                                                    importIPS32Patch,
                                                    ImHexApi::Provider::isValid);

            /* Modified File */
            ContentRegistry::Interface::addMenuItem({ "hex.builtin.menu.file", "hex.builtin.menu.file.import", "hex.builtin.menu.file.import.modified_file" }, 2300,
                                                    Shortcut::None,
                                                    importModifiedFile,
                                                    [&] { return noRunningTaskAndValidProvider() && ImHexApi::Provider::get()->isWritable(); });
        }

        /* Export */
        {
            /* Base 64 */
            ContentRegistry::Interface::addMenuItem({ "hex.builtin.menu.file", "hex.builtin.menu.file.export", "hex.builtin.menu.file.export.base64" }, 6000,
                                                    Shortcut::None,
                                                    exportBase64,
                                                    ImHexApi::Provider::isValid);

            ContentRegistry::Interface::addMenuItemSeparator({ "hex.builtin.menu.file", "hex.builtin.menu.file.export" }, 6050);

            /* IPS */
            ContentRegistry::Interface::addMenuItem({ "hex.builtin.menu.file", "hex.builtin.menu.file.export", "hex.builtin.menu.file.export.ips" }, 6100,
                                                    Shortcut::None,
                                                    exportIPSPatch,
                                                    ImHexApi::Provider::isValid);

            /* IPS32 */
            ContentRegistry::Interface::addMenuItem({ "hex.builtin.menu.file", "hex.builtin.menu.file.export", "hex.builtin.menu.file.export.ips32" }, 6150,
                                                    Shortcut::None,
                                                    exportIPS32Patch,
                                                    ImHexApi::Provider::isValid);
        }

        ContentRegistry::Interface::addMenuItemSeparator({ "hex.builtin.menu.file" }, 10000);

        /* Close Provider */
        ContentRegistry::Interface::addMenuItem({ "hex.builtin.menu.file", "hex.builtin.menu.file.close"}, 10050, CTRLCMD + Keys::W, [] {
            ImHexApi::Provider::remove(ImHexApi::Provider::get());
        }, noRunningTaskAndValidProvider);

        /* Quit ImHex */
        ContentRegistry::Interface::addMenuItem({ "hex.builtin.menu.file", "hex.builtin.menu.file.quit"}, 10100, ALT + Keys::F4, [] {
            ImHexApi::System::closeImHex();
        });
    }

    static void createEditMenu() {
        ContentRegistry::Interface::registerMainMenuItem("hex.builtin.menu.edit", 2000);

        /* Undo */
        ContentRegistry::Interface::addMenuItem({ "hex.builtin.menu.edit", "hex.builtin.menu.edit.undo" }, 1000, CTRLCMD + Keys::Z, [] {
            auto provider = ImHexApi::Provider::get();
                provider->undo();
        }, [&] { return ImHexApi::Provider::isValid() && ImHexApi::Provider::get()->canUndo(); });

        /* Redo */
        ContentRegistry::Interface::addMenuItem({ "hex.builtin.menu.edit", "hex.builtin.menu.edit.redo" }, 1050, CTRLCMD + Keys::Y, [] {
            auto provider = ImHexApi::Provider::get();
                provider->redo();
        }, [&] { return ImHexApi::Provider::isValid() && ImHexApi::Provider::get()->canRedo(); });

    }

    static void createViewMenu() {
        ContentRegistry::Interface::registerMainMenuItem("hex.builtin.menu.view", 3000);

        ContentRegistry::Interface::addMenuItemSubMenu({ "hex.builtin.menu.view" }, 1000, [] {
            for (auto &[name, view] : ContentRegistry::Views::impl::getEntries()) {
                if (view->hasViewMenuItemEntry())
                    ImGui::MenuItem(LangEntry(view->getUnlocalizedName()), "", &view->getWindowOpenState());
            }

            ImGui::Separator();

            #if defined(DEBUG)
                ImGui::MenuItem("hex.builtin.menu.view.demo"_lang, "", &g_demoWindowOpen);
            #endif
        });

    }

    static void createLayoutMenu() {
        ContentRegistry::Interface::registerMainMenuItem("hex.builtin.menu.layout", 4000);

        ContentRegistry::Interface::addMenuItemSubMenu({ "hex.builtin.menu.layout" }, 1000, [] {
            for (auto &[layoutName, func] : ContentRegistry::Interface::impl::getLayouts()) {
                if (ImGui::MenuItem(LangEntry(layoutName), "", false, ImHexApi::Provider::isValid())) {
                    auto dock = ImHexApi::System::getMainDockSpaceId();

                    for (auto &[viewName, view] : ContentRegistry::Views::impl::getEntries()) {
                        view->getWindowOpenState() = false;
                    }

                    ImGui::DockBuilderRemoveNode(dock);
                    ImGui::DockBuilderAddNode(dock);
                    func(dock);
                    ImGui::DockBuilderFinish(dock);
                }
            }
        });
    }

    static void createHelpMenu() {
        ContentRegistry::Interface::registerMainMenuItem("hex.builtin.menu.help", 5000);

        ContentRegistry::Interface::addMenuItem({ "hex.builtin.menu.help", "hex.builtin.menu.help.ask_for_help" }, 5000, CTRLCMD + SHIFT + Keys::D, [] {
            PopupDocsQuestion::open();
        });
    }

    void registerMainMenuEntries() {
        createFileMenu();
        createEditMenu();
        createViewMenu();
        createLayoutMenu();
        createHelpMenu();

        (void)EventManager::subscribe<EventFrameEnd>([] {
            if (g_demoWindowOpen) {
                ImGui::ShowDemoWindow(&g_demoWindowOpen);
                ImPlot::ShowDemoWindow(&g_demoWindowOpen);
            }
        });
    }

}