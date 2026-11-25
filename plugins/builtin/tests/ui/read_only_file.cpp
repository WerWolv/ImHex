#include <hex/test/tests.hpp>

#if defined(OS_LINUX) || defined(OS_MACOS) || defined(OS_FREEBSD)

#include <hex/api/events/events_interaction.hpp>
#include <hex/api/imhex_api/provider.hpp>
#include <hex/api/localization_manager.hpp>
#include <hex/api/task_manager.hpp>
#include <hex/providers/provider.hpp>
#include <wolv/io/file.hpp>

#include <chrono>
#include <random>
#include <unistd.h>

namespace hex::plugin::builtin {

    namespace {

        // Clean up even if an assertion aborts the test.
        class ReadOnlyFile {
        public:
            ReadOnlyFile() {
                auto directory = std::fs::temp_directory_path() /
                    fmt::format("imhex-ui-read-only-{}", std::random_device{}());
                if (std::fs::create_directory(directory))
                    m_directory = std::move(directory);
            }

            ~ReadOnlyFile() {
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

            std::fs::path create(const std::vector<u8> &bytes) const {
                if (m_directory.empty())
                    return {};

                const auto path = m_directory / "read-only.bin";
                {
                    wolv::io::File file(path, wolv::io::File::Mode::Create);
                    if (!file.isValid() || file.writeVector(bytes) != static_cast<i64>(bytes.size()))
                        return {};
                }
                std::error_code error;
                std::fs::permissions(path, std::fs::perms::owner_read, std::fs::perm_options::replace, error);
                return error ? std::fs::path{} : path;
            }

        private:
            std::fs::path m_directory;
        };

    }

    // Verify both the editor and edit commands reject changes to a read-only file.
    IMGUI_TEST_SEQUENCE("Hex Editor (injected file setup)", "Read-only file cannot be edited", ctx) {
        // Root can bypass file permission bits.
        if (geteuid() == 0)
            return;

        IM_CHECK(ImHexApi::Provider::getProviders().empty());
        const ReadOnlyFile file;
        const std::vector<u8> original = { 0x11, 0x22, 0x33 };
        const auto path = file.create(original);
        IM_CHECK(!path.empty());
        TaskManager::doLater([path] { EventFileDropped::post(path); });

        prv::Provider *provider = nullptr;
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        do {
            ctx->Yield();
            provider = ImHexApi::Provider::get();
            const auto *picked = dynamic_cast<prv::IProviderFilePicker *>(provider);
            if (TaskManager::getRunningTaskCount() == 0 && picked != nullptr &&
                picked->getPickedPath() == path && provider->isAvailable())
                break;
            provider = nullptr;
        } while (!ctx->IsError() && std::chrono::steady_clock::now() < deadline);

        IM_CHECK(provider != nullptr);
        IM_CHECK(provider->isReadable() && !provider->isWritable() && !provider->isResizable());
        ctx->SetRef("###hex.builtin.view.hex_editor.name");
        ctx->ItemDoubleClick("**/hex-byte-1", ImGuiTestOpFlags_NoCheckHoveredId);
        ctx->KeyChars("A5");
        IM_CHECK(!provider->isDataDirty());
        IM_CHECK(provider->getActualSize() == original.size());
        std::vector<u8> bytes(original.size());
        provider->read(0, bytes.data(), bytes.size());
        IM_CHECK(bytes == original);

        const auto editMenu = fmt::format("//##MainMenuBar/{}", "hex.builtin.menu.edit"_lang);
        ctx->MenuAction(ImGuiTestAction_Open, editMenu.c_str());
        for (const auto *item : { "hex.builtin.view.hex_editor.menu.edit.insert",
                                  "hex.builtin.view.hex_editor.menu.edit.remove",
                                  "hex.builtin.view.hex_editor.menu.edit.fill" }) {
            const auto itemPath = fmt::format("//$FOCUSED/{}", Lang(item).get());
            IM_CHECK((ctx->ItemInfo(itemPath.c_str()).ItemFlags & ImGuiItemFlags_Disabled) != 0);
        }
        ctx->KeyPress(ImGuiKey_Escape);
        ctx->MenuClick(fmt::format("//##MainMenuBar/{}/{}", "hex.builtin.menu.file"_lang,
                                   "hex.builtin.menu.file.close"_lang).c_str());
        IM_CHECK(ImHexApi::Provider::getProviders().empty());
        wolv::io::File saved(path, wolv::io::File::Mode::Read);
        IM_CHECK(saved.isValid());
        IM_CHECK(saved.readVector() == original);
    };

}

#endif
