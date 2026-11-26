#pragma once

#include <hex/test/tests.hpp>

#include <hex/api/events/events_interaction.hpp>
#include <hex/api/events/events_provider.hpp>
#include <hex/api/imhex_api/provider.hpp>
#include <hex/api/localization_manager.hpp>
#include <hex/api/task_manager.hpp>
#include <hex/providers/provider.hpp>

#include <wolv/io/file.hpp>

#include <chrono>
#include <random>

namespace hex::plugin::builtin::ui_test {

    // Isolate each test's files, including any backups written by Save.
    class TestFiles {
    public:
        // Give each fixture its own temporary directory.
        TestFiles() {
            auto directory = std::fs::temp_directory_path() /
                fmt::format("imhex-ui-test-{}", std::random_device{}());
            if (std::fs::create_directory(directory))
                m_directory = std::move(directory);
        }

        // Clean up only if this fixture owns a directory.
        ~TestFiles() {
            if (m_directory.empty())
                return;

            // Close providers before deleting their files.
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

        // Keep cleanup ownership with one fixture.
        TestFiles(const TestFiles &) = delete;
        TestFiles &operator=(const TestFiles &) = delete;

        const std::fs::path &directory() const { return m_directory; }

        // Create an input file, returning no path if setup fails.
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

    // Inject file opening as setup, bypassing the native dialog and actual drag-and-drop.
    inline prv::Provider *openFileByInjectedDrop(ImGuiTestContext *ctx, const std::fs::path &path) {
        if (path.empty())
            return nullptr;

        prv::Provider *opened = nullptr;
        const auto subscription = EventProviderOpened::subscribe([&](prv::Provider *provider) {
            const auto *file = dynamic_cast<prv::IProviderFilePicker *>(provider);
            if (file != nullptr && file->getPickedPath() == path)
                opened = provider;
        });
        TaskManager::doLater([path] { EventFileDropped::post(path); });

        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        do {
            ctx->Yield();
            if (opened != nullptr && TaskManager::getRunningTaskCount() == 0)
                break;
        } while (!ctx->IsError() && std::chrono::steady_clock::now() < deadline);

        EventManager::unsubscribe(subscription);
        return !ctx->IsError() && TaskManager::getRunningTaskCount() == 0 && opened != nullptr &&
               opened->isAvailable() ? opened : nullptr;
    }

    // Select a command from the main menu bar.
    inline void clickMenu(ImGuiTestContext *ctx, const char *menu, const char *item) {
        ctx->MenuClick(fmt::format("//##MainMenuBar/{}/{}", menu, item).c_str());
    }

    // Exercise the same close command a user would.
    inline void closeFile(ImGuiTestContext *ctx) {
        clickMenu(ctx, "hex.builtin.menu.file"_lang, "hex.builtin.menu.file.close"_lang);
    }

    // Check the length too, so an edit cannot leave trailing bytes unnoticed.
    inline bool hasBytes(prv::Provider &provider, const std::vector<u8> &expected) {
        if (provider.getActualSize() != expected.size())
            return false;

        std::vector<u8> bytes(expected.size());
        if (!bytes.empty())
            provider.read(0, bytes.data(), bytes.size());
        return bytes == expected;
    }

}
