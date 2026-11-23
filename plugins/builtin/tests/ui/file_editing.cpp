#include <hex/test/tests.hpp>

#include <hex/api/events/events_interaction.hpp>
#include <hex/api/imhex_api/provider.hpp>
#include <hex/api/localization_manager.hpp>
#include <hex/api/task_manager.hpp>
#include <hex/providers/provider.hpp>

#include <wolv/io/file.hpp>
#include <wolv/utils/guards.hpp>

#include <chrono>
#include <random>

namespace hex::plugin::builtin {

    // Drop a real file, edit its last byte, and verify the saved file after closing it.
    IMGUI_TEST_SEQUENCE("Hex Editor", "Open, overwrite, save and close a file", ctx) {
        IM_CHECK(ImHexApi::Provider::getProviders().empty());

        const auto directory = std::fs::temp_directory_path() /
            fmt::format("imhex-ui-test-{}", std::random_device{}());
        IM_CHECK(std::fs::create_directory(directory));
        const auto path = directory / "edit.bin";

        ON_SCOPE_EXIT {
            TaskManager::doLater([path, directory] {
                for (auto *provider : ImHexApi::Provider::getProviders()) {
                    auto *file = dynamic_cast<prv::IProviderFilePicker *>(provider);
                    if (file != nullptr && file->getPickedPath() == path)
                        ImHexApi::Provider::remove(provider, true);
                }
                std::error_code error;
                std::fs::remove_all(directory, error);
            });
        };

        {
            wolv::io::File file(path, wolv::io::File::Mode::Create);
            IM_CHECK(file.isValid());
            IM_CHECK(file.writeVector({ 0x11, 0x22, 0x33 }) == 3);
        }

        // ImGui cannot drive the OS file picker; use the application's drop entry point.
        TaskManager::doLater([path] { EventFileDropped::post(path); });
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        auto *provider = ImHexApi::Provider::get();
        while ((provider == nullptr || TaskManager::getRunningTaskCount() > 0) &&
                std::chrono::steady_clock::now() < deadline && !ctx->IsError()) {
            ctx->Yield();
            provider = ImHexApi::Provider::get();
        }

        IM_CHECK(TaskManager::getRunningTaskCount() == 0);
        const auto *openedFile = dynamic_cast<prv::IProviderFilePicker *>(provider);
        IM_CHECK(openedFile != nullptr && openedFile->getPickedPath() == path);
        IM_CHECK(provider->isWritable());
        IM_CHECK(provider->getActualSize() == 3);

        ctx->SetRef("###hex.builtin.view.hex_editor.name");
        ctx->ItemDoubleClick("**/hex-byte-2", ImGuiTestOpFlags_NoCheckHoveredId);
        ctx->KeyChars("A5");
        IM_CHECK(provider->isDataDirty());

        const auto fileMenu = fmt::format("//##MainMenuBar/{}/", "hex.builtin.menu.file"_lang);
        ctx->MenuClick((fileMenu + "hex.builtin.view.hex_editor.menu.file.save"_lang.get()).c_str());
        IM_CHECK(!provider->isDataDirty());

        ctx->MenuClick((fileMenu + "hex.builtin.menu.file.close"_lang.get()).c_str());
        IM_CHECK(ImHexApi::Provider::getProviders().empty());

        wolv::io::File saved(path, wolv::io::File::Mode::Read);
        const std::vector<u8> expected = { 0x11, 0x22, 0xA5 };
        IM_CHECK(saved.isValid());
        IM_CHECK(saved.readVector() == expected);
    };

}
