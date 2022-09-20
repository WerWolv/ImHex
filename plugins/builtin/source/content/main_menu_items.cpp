#include <hex/api/content_registry.hpp>

#include <imgui.h>
#include <implot.h>

#include <hex/ui/view.hpp>
#include <hex/api/project_file_manager.hpp>
#include <hex/helpers/file.hpp>
#include <hex/helpers/crypto.hpp>
#include <hex/helpers/patches.hpp>

namespace hex::plugin::builtin {

    static bool g_demoWindowOpen = false;

    static void createFileMenu() {

        ContentRegistry::Interface::registerMainMenuItem("hex.builtin.menu.file", 1000);

        ContentRegistry::Interface::addMenuItem("hex.builtin.menu.file", 1050, [&] {
            bool taskRunning = TaskManager::getRunningTaskCount() > 0;

            if (ImGui::MenuItem("hex.builtin.menu.file.create_file"_lang, "CTRL + N", false, !taskRunning)) {
                EventManager::post<RequestOpenWindow>("Create File");
            }

            if (ImGui::MenuItem("hex.builtin.menu.file.open_file"_lang, "CTRL + O", false, !taskRunning)) {
                EventManager::post<RequestOpenWindow>("Open File");
            }

            if (ImGui::BeginMenu("hex.builtin.menu.file.open_other"_lang, !taskRunning)) {

                for (const auto &unlocalizedProviderName : ContentRegistry::Provider::getEntries()) {
                    if (ImGui::MenuItem(LangEntry(unlocalizedProviderName))) {
                        ImHexApi::Provider::createProvider(unlocalizedProviderName);
                    }
                }

                ImGui::EndMenu();
            }
        });

        /* File open, quit imhex */
        ContentRegistry::Interface::addMenuItem("hex.builtin.menu.file", 1150, [&] {
            bool providerValid = ImHexApi::Provider::isValid();
            bool taskRunning = TaskManager::getRunningTaskCount() > 0;

            if (ImGui::MenuItem("hex.builtin.menu.file.close"_lang, "CTRL + W", false, providerValid && !taskRunning)) {
                ImHexApi::Provider::remove(ImHexApi::Provider::get());
            }

            if (ImGui::MenuItem("hex.builtin.menu.file.quit"_lang)) {
                ImHexApi::Common::closeImHex();
            }
        });

        /* Project open / save */
        ContentRegistry::Interface::addMenuItem("hex.builtin.menu.file", 1250, [&] {
            auto provider      = ImHexApi::Provider::get();
            bool providerValid = ImHexApi::Provider::isValid();
            bool taskRunning = TaskManager::getRunningTaskCount() > 0;

            if (ImGui::MenuItem("hex.builtin.menu.file.open_project"_lang, "", false, !taskRunning)) {
                fs::openFileBrowser(fs::DialogMode::Open, { {"Project File", "hexproj"}
                },
                    [](const auto &path) {
                        if (!ProjectFile::load(path)) {
                            View::showErrorPopup("hex.builtin.popup.error.project.load"_lang);
                        }
                    });
            }

            if (ImGui::MenuItem("hex.builtin.menu.file.save_project"_lang, "", false, providerValid && provider->isWritable())) {
                fs::openFileBrowser(fs::DialogMode::Save, { {"Project File", "hexproj"} },
                    [](std::fs::path path) {
                        if (path.extension() != ".hexproj") {
                            path.replace_extension(".hexproj");
                        }

                        if (!ProjectFile::load(path)) {
                            View::showErrorPopup("hex.builtin.popup.error.project.load"_lang);
                        }
                    });
            }
        });

        /* Import / Export */
        ContentRegistry::Interface::addMenuItem("hex.builtin.menu.file", 1300, [&] {
            auto provider      = ImHexApi::Provider::get();
            bool providerValid = ImHexApi::Provider::isValid();
            bool taskRunning = TaskManager::getRunningTaskCount() > 0;

            /* Import */
            if (ImGui::BeginMenu("hex.builtin.menu.file.import"_lang, !taskRunning)) {
                if (ImGui::MenuItem("hex.builtin.menu.file.import.base64"_lang)) {

                    fs::openFileBrowser(fs::DialogMode::Open, {}, [](const auto &path) {
                        fs::File inputFile(path, fs::File::Mode::Read);
                        if (!inputFile.isValid()) {
                            View::showErrorPopup("hex.builtin.menu.file.import.base64.popup.open_error"_lang);
                            return;
                        }

                        auto base64 = inputFile.readBytes();

                        if (!base64.empty()) {
                            auto data = crypt::decode64(base64);

                            if (data.empty())
                                View::showErrorPopup("hex.builtin.menu.file.import.base64.popup.import_error"_lang);
                            else {
                                fs::openFileBrowser(fs::DialogMode::Save, {}, [&data](const std::fs::path &path) {
                                    fs::File outputFile(path, fs::File::Mode::Create);

                                    if (!outputFile.isValid())
                                        View::showErrorPopup("hex.builtin.menu.file.import.base64.popup.import_error"_lang);

                                    outputFile.write(data);
                                });
                            }
                        } else {
                            View::showErrorPopup("hex.builtin.popup.file_open_error"_lang);
                        }
                    });
                }

                ImGui::Separator();

                if (ImGui::MenuItem("hex.builtin.menu.file.import.ips"_lang, nullptr, false)) {

                    fs::openFileBrowser(fs::DialogMode::Open, {}, [](const auto &path) {
                        TaskManager::createTask("hex.builtin.common.processing", TaskManager::NoProgress, [path](auto &task) {
                            auto patchData = fs::File(path, fs::File::Mode::Read).readBytes();
                            auto patch     = hex::loadIPSPatch(patchData);

                            task.setMaxValue(patch.size());

                            auto provider = ImHexApi::Provider::get();

                            u64 progress = 0;
                            for (auto &[address, value] : patch) {
                                provider->addPatch(address, &value, 1);
                                progress++;
                                task.update(progress);
                            }

                            provider->createUndoPoint();
                        });
                    });
                }

                if (ImGui::MenuItem("hex.builtin.menu.file.import.ips32"_lang, nullptr, false)) {
                    fs::openFileBrowser(fs::DialogMode::Open, {}, [](const auto &path) {
                        TaskManager::createTask("hex.builtin.common.processing", TaskManager::NoProgress, [path](auto &task) {
                            auto patchData = fs::File(path, fs::File::Mode::Read).readBytes();
                            auto patch     = hex::loadIPS32Patch(patchData);

                            task.setMaxValue(patch.size());

                            auto provider = ImHexApi::Provider::get();

                            u64 progress = 0;
                            for (auto &[address, value] : patch) {
                                provider->addPatch(address, &value, 1);
                                progress++;
                                task.update(progress);
                            }

                            provider->createUndoPoint();
                        });
                    });
                }

                ImGui::EndMenu();
            }


            /* Export */
            if (ImGui::BeginMenu("hex.builtin.menu.file.export"_lang, providerValid && provider->isWritable())) {
                if (ImGui::MenuItem("hex.builtin.menu.file.export.ips"_lang, nullptr, false)) {
                    Patches patches = provider->getPatches();
                    if (!patches.contains(0x00454F45) && patches.contains(0x00454F46)) {
                        u8 value = 0;
                        provider->read(0x00454F45, &value, sizeof(u8));
                        patches[0x00454F45] = value;
                    }

                    TaskManager::createTask("hex.builtin.common.processing", TaskManager::NoProgress, [patches](auto &) {
                        auto data = generateIPSPatch(patches);

                        TaskManager::doLater([data] {
                            fs::openFileBrowser(fs::DialogMode::Save, {}, [&data](const auto &path) {
                                auto file = fs::File(path, fs::File::Mode::Create);
                                if (!file.isValid()) {
                                    View::showErrorPopup("hex.builtin.menu.file.export.base64.popup.export_error"_lang);
                                    return;
                                }

                                file.write(data);
                            });
                        });
                    });
                }

                if (ImGui::MenuItem("hex.builtin.menu.file.export.ips32"_lang, nullptr, false)) {
                    Patches patches = provider->getPatches();
                    if (!patches.contains(0x00454F45) && patches.contains(0x45454F46)) {
                        u8 value = 0;
                        provider->read(0x45454F45, &value, sizeof(u8));
                        patches[0x45454F45] = value;
                    }

                    TaskManager::createTask("hex.builtin.common.processing", TaskManager::NoProgress, [patches](auto &) {
                        auto data = generateIPS32Patch(patches);

                        TaskManager::doLater([data] {
                            fs::openFileBrowser(fs::DialogMode::Save, {}, [&data](const auto &path) {
                                auto file = fs::File(path, fs::File::Mode::Create);
                                if (!file.isValid()) {
                                    View::showErrorPopup("hex.builtin.menu.file.export.base64.popup.export_error"_lang);
                                    return;
                                }

                                file.write(data);
                            });
                        });
                    });
                }

                ImGui::EndMenu();
            }
        });
    }

    static void createEditMenu() {
        ContentRegistry::Interface::registerMainMenuItem("hex.builtin.menu.edit", 2000);

        /* Provider Undo / Redo */
        ContentRegistry::Interface::addMenuItem("hex.builtin.menu.edit", 1000, [&] {
            auto provider      = ImHexApi::Provider::get();
            bool providerValid = ImHexApi::Provider::isValid();

            if (ImGui::MenuItem("hex.builtin.menu.edit.undo"_lang, "CTRL + Z", false, providerValid && provider->canUndo()))
                provider->undo();
            if (ImGui::MenuItem("hex.builtin.menu.edit.redo"_lang, "CTRL + Y", false, providerValid && provider->canRedo()))
                provider->redo();
        });

    }

    static void createViewMenu() {
        ContentRegistry::Interface::registerMainMenuItem("hex.builtin.menu.view", 3000);

        ContentRegistry::Interface::addMenuItem("hex.builtin.menu.view", 1000, [] {
            for (auto &[name, view] : ContentRegistry::Views::getEntries()) {
                if (view->hasViewMenuItemEntry())
                    ImGui::MenuItem(LangEntry(view->getUnlocalizedName()), "", &view->getWindowOpenState());
            }
        });

        #if defined(DEBUG)
            ContentRegistry::Interface::addMenuItem("hex.builtin.menu.view", 2000, [] {
                ImGui::MenuItem("hex.builtin.menu.view.demo"_lang, "", &g_demoWindowOpen);
            });
        #endif
    }

    static void createLayoutMenu() {
        ContentRegistry::Interface::registerMainMenuItem("hex.builtin.menu.layout", 4000);

        ContentRegistry::Interface::addMenuItem("hex.builtin.menu.layout", 1000, [] {
            for (auto &[layoutName, func] : ContentRegistry::Interface::getLayouts()) {
                if (ImGui::MenuItem(LangEntry(layoutName), "", false, ImHexApi::Provider::isValid())) {
                    auto dock = ImHexApi::System::getMainDockSpaceId();

                    for (auto &[viewName, view] : ContentRegistry::Views::getEntries()) {
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