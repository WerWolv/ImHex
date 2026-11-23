#include <hex/test/tests.hpp>

#include <hex/api/events/events_interaction.hpp>
#include <hex/api/imhex_api/provider.hpp>
#include <hex/api/localization_manager.hpp>
#include <hex/api/task_manager.hpp>
#include <hex/providers/provider.hpp>

#include <wolv/io/file.hpp>

#include <chrono>
#include <random>

namespace hex::plugin::builtin {

    namespace {

        // Keep each test's files and any save backups in a private directory.
        class TestFiles {
        public:
            TestFiles() {
                auto directory = std::fs::temp_directory_path() /
                    fmt::format("imhex-ui-test-{}", std::random_device{}());
                if (std::fs::create_directory(directory))
                    m_directory = std::move(directory);
            }

            ~TestFiles() {
                if (m_directory.empty())
                    return;

                TaskManager::doLater([directory = m_directory] {
                    for (auto *provider : ImHexApi::Provider::getProviders()) {
                        auto *file = dynamic_cast<prv::IProviderFilePicker *>(provider);
                        if (file != nullptr && file->getPickedPath().parent_path() == directory)
                            ImHexApi::Provider::remove(provider, true);
                    }
                    std::error_code error;
                    std::fs::remove_all(directory, error);
                });
            }

            std::fs::path create(const char *name, const std::vector<u8> &bytes) const {
                if (m_directory.empty())
                    return {};

                const auto path = m_directory / name;
                wolv::io::File file(path, wolv::io::File::Mode::Create);
                if (!file.isValid() || file.writeVector(bytes) != static_cast<i64>(bytes.size()))
                    return {};

                return path;
            }

        private:
            std::fs::path m_directory;
        };

        // This sets up the editor; it does not test a GUI file-opening action.
        prv::Provider *openFileByInjectedDrop(ImGuiTestContext *ctx, const std::fs::path &path) {
            TaskManager::doLater([path] { EventFileDropped::post(path); });
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
            do {
                ctx->Yield();
                auto *provider = ImHexApi::Provider::get();
                const auto *file = dynamic_cast<prv::IProviderFilePicker *>(provider);
                if (TaskManager::getRunningTaskCount() == 0 && file != nullptr &&
                    file->getPickedPath() == path && provider->isAvailable())
                    return provider;
            } while (!ctx->IsError() && std::chrono::steady_clock::now() < deadline);

            return nullptr;
        }

        void clickMenu(ImGuiTestContext *ctx, const char *menu, const char *item) {
            ctx->MenuClick(fmt::format("//##MainMenuBar/{}/{}", menu, item).c_str());
        }

        void closeFile(ImGuiTestContext *ctx) {
            clickMenu(ctx, "hex.builtin.menu.file"_lang, "hex.builtin.menu.file.close"_lang);
        }

        // Check the length too, so an edit cannot leave trailing bytes unnoticed.
        bool hasBytes(prv::Provider &provider, const std::vector<u8> &expected) {
            if (provider.getActualSize() != expected.size())
                return false;

            std::vector<u8> bytes(expected.size());
            provider.read(0, bytes.data(), bytes.size());
            return bytes == expected;
        }

    }

    // Edit the last byte and verify the saved file after closing it.
    IMGUI_TEST_SEQUENCE("Hex Editor (injected file setup)", "Overwrite, save and close a file", ctx) {
        IM_CHECK(ImHexApi::Provider::getProviders().empty());

        const TestFiles files;
        const auto path = files.create("edit.bin", { 0x11, 0x22, 0x33 });
        IM_CHECK(!path.empty());
        auto *provider = openFileByInjectedDrop(ctx, path);
        IM_CHECK(provider != nullptr && provider->isWritable());
        IM_CHECK(provider->getActualSize() == 3);

        ctx->SetRef("###hex.builtin.view.hex_editor.name");
        ctx->ItemDoubleClick("**/hex-byte-2", ImGuiTestOpFlags_NoCheckHoveredId);
        ctx->KeyChars("A5");
        IM_CHECK(provider->isDataDirty());

        clickMenu(ctx, "hex.builtin.menu.file"_lang, "hex.builtin.view.hex_editor.menu.file.save"_lang);
        IM_CHECK(!provider->isDataDirty());

        closeFile(ctx);
        IM_CHECK(ImHexApi::Provider::getProviders().empty());

        wolv::io::File saved(path, wolv::io::File::Mode::Read);
        const std::vector<u8> expected = { 0x11, 0x22, 0xA5 };
        IM_CHECK(saved.isValid());
        IM_CHECK(saved.readVector() == expected);
    };

    // Closing the selected tab should leave the other file open and selected.
    IMGUI_TEST_SEQUENCE("Hex Editor (injected file setup)", "Switch between two files and close them", ctx) {
        IM_CHECK(ImHexApi::Provider::getProviders().empty());

        const TestFiles files;
        const auto firstPath = files.create("first.bin", { 0x11 });
        const auto secondPath = files.create("second.bin", { 0x22 });
        IM_CHECK(!firstPath.empty() && !secondPath.empty());
        auto *first = openFileByInjectedDrop(ctx, firstPath);
        IM_CHECK(first != nullptr);
        auto *second = openFileByInjectedDrop(ctx, secondPath);
        IM_CHECK(second != nullptr && second != first);
        IM_CHECK(ImHexApi::Provider::getProviders().size() == 2);
        IM_CHECK(ImHexApi::Provider::get() == second);

        const auto firstTab = fmt::format("//ImHexDockSpace/**/{} {}", first->getIcon(), first->getName());
        ctx->ItemClick(firstTab.c_str());
        ctx->Yield();
        IM_CHECK(ImHexApi::Provider::get() == first);

        closeFile(ctx);
        IM_CHECK(ImHexApi::Provider::getProviders().size() == 1);
        IM_CHECK(ImHexApi::Provider::get() == second);
        IM_CHECK(hasBytes(*second, { 0x22 }));

        closeFile(ctx);
        IM_CHECK(ImHexApi::Provider::getProviders().empty());
    };

    // Insert in the middle so the test checks that the following bytes move.
    IMGUI_TEST_SEQUENCE("Hex Editor (injected file setup)", "Insert bytes, undo and redo", ctx) {
        IM_CHECK(ImHexApi::Provider::getProviders().empty());

        const TestFiles files;
        const std::vector<u8> original = { 0x11, 0x22, 0x33 };
        const auto path = files.create("insert.bin", original);
        IM_CHECK(!path.empty());
        auto *provider = openFileByInjectedDrop(ctx, path);
        IM_CHECK(provider != nullptr && provider->isResizable());

        ctx->SetRef("###hex.builtin.view.hex_editor.name");
        ctx->ItemClick("**/hex-byte-1", ImGuiMouseButton_Left, ImGuiTestOpFlags_NoCheckHoveredId);
        clickMenu(ctx, "hex.builtin.menu.edit"_lang, "hex.builtin.view.hex_editor.menu.edit.insert"_lang);

        ctx->SetRef("##hex.builtin.view.hex_editor.menu.edit.insert");
        ctx->ItemClick("hex.ui.common.size"_lang.get());
        ctx->KeyCharsReplace("2");
        ctx->ItemClick("hex.ui.common.set"_lang.get());

        const std::vector<u8> inserted = { 0x11, 0x00, 0x00, 0x22, 0x33 };
        IM_CHECK(hasBytes(*provider, inserted));

        clickMenu(ctx, "hex.builtin.menu.edit"_lang, "hex.builtin.view.hex_editor.menu.edit.undo"_lang);
        IM_CHECK(hasBytes(*provider, original));

        clickMenu(ctx, "hex.builtin.menu.edit"_lang, "hex.builtin.view.hex_editor.menu.edit.redo"_lang);
        IM_CHECK(hasBytes(*provider, inserted));

        clickMenu(ctx, "hex.builtin.menu.file"_lang, "hex.builtin.view.hex_editor.menu.file.save"_lang);
        closeFile(ctx);
        IM_CHECK(ImHexApi::Provider::getProviders().empty());
        wolv::io::File saved(path, wolv::io::File::Mode::Read);
        IM_CHECK(saved.isValid());
        IM_CHECK(saved.readVector() == inserted);
    };

    IMGUI_TEST_SEQUENCE("Hex Editor (injected file setup)", "Remove a byte, undo and redo", ctx) {
        IM_CHECK(ImHexApi::Provider::getProviders().empty());
        const TestFiles files;
        const std::vector<u8> original = { 0x11, 0x22, 0x33 };
        const auto path = files.create("remove.bin", original);
        IM_CHECK(!path.empty());
        auto *provider = openFileByInjectedDrop(ctx, path);
        IM_CHECK(provider != nullptr && provider->isResizable());

        ctx->SetRef("###hex.builtin.view.hex_editor.name");
        ctx->ItemClick("**/hex-byte-1", ImGuiMouseButton_Left, ImGuiTestOpFlags_NoCheckHoveredId);
        clickMenu(ctx, "hex.builtin.menu.edit"_lang, "hex.builtin.view.hex_editor.menu.edit.remove"_lang);
        ctx->SetRef("##hex.builtin.view.hex_editor.menu.edit.remove");
        ctx->ItemClick("hex.ui.common.set"_lang.get());
        IM_CHECK(hasBytes(*provider, { 0x11, 0x33 }));
        clickMenu(ctx, "hex.builtin.menu.edit"_lang, "hex.builtin.view.hex_editor.menu.edit.undo"_lang);
        IM_CHECK(hasBytes(*provider, original));
        clickMenu(ctx, "hex.builtin.menu.edit"_lang, "hex.builtin.view.hex_editor.menu.edit.redo"_lang);
        IM_CHECK(hasBytes(*provider, { 0x11, 0x33 }));
        clickMenu(ctx, "hex.builtin.menu.file"_lang, "hex.builtin.view.hex_editor.menu.file.save"_lang);
        closeFile(ctx);
        IM_CHECK(ImHexApi::Provider::getProviders().empty());
        wolv::io::File saved(path, wolv::io::File::Mode::Read);
        IM_CHECK(saved.isValid());
        IM_CHECK(saved.readVector() == std::vector<u8>({ 0x11, 0x33 }));
    };

    IMGUI_TEST_SEQUENCE("Hex Editor (injected file setup)", "Copy and paste a byte", ctx) {
        IM_CHECK(ImHexApi::Provider::getProviders().empty());
        const TestFiles files;
        const std::vector<u8> original = { 0x11, 0x22, 0x33 };
        const auto path = files.create("copy-paste.bin", original);
        IM_CHECK(!path.empty());
        auto *provider = openFileByInjectedDrop(ctx, path);
        IM_CHECK(provider != nullptr && provider->isWritable());

        ctx->SetRef("###hex.builtin.view.hex_editor.name");
        ctx->ItemClick("**/hex-byte-0", ImGuiMouseButton_Left, ImGuiTestOpFlags_NoCheckHoveredId);
        clickMenu(ctx, "hex.builtin.menu.edit"_lang, "hex.builtin.view.hex_editor.menu.edit.copy"_lang);
        ctx->ItemClick("**/hex-byte-2", ImGuiMouseButton_Left, ImGuiTestOpFlags_NoCheckHoveredId);
        ctx->MenuClick(fmt::format("//##MainMenuBar/{}/{}/{}",
            "hex.builtin.menu.edit"_lang, "hex.builtin.view.hex_editor.menu.edit.paste_as"_lang,
            "hex.builtin.view.hex_editor.menu.edit.paste_all"_lang).c_str());
        IM_CHECK(hasBytes(*provider, { 0x11, 0x22, 0x11 }));
        clickMenu(ctx, "hex.builtin.menu.edit"_lang, "hex.builtin.view.hex_editor.menu.edit.undo"_lang);
        IM_CHECK(hasBytes(*provider, original));
        clickMenu(ctx, "hex.builtin.menu.file"_lang, "hex.builtin.view.hex_editor.menu.file.save"_lang);
        closeFile(ctx);
        IM_CHECK(ImHexApi::Provider::getProviders().empty());
    };

}
