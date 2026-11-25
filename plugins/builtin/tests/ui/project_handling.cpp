#include "ui_test_helpers.hpp"

#include <fonts/vscode_icons.hpp>
#include <hex/api/content_registry/views.hpp>
#include <hex/api/events/events_lifecycle.hpp>
#include <hex/api/imhex_api/hex_editor.hpp>
#include <hex/api/project_manager.hpp>

#include <chrono>

namespace hex::plugin::builtin {

    using namespace ui_test;

    namespace {

        // Restore the temporary project on early failure, before TestFiles removes the fixture.
        struct ProjectCleanup {
            std::fs::path path;

            ~ProjectCleanup() {
                if (ProjectManager::getPath() != path)
                    return;

                TaskManager::doLater([path = path] {
                    if (ProjectManager::getPath() != path)
                        return;

                    ProjectManager::clearPath();
                    EventProjectClosed::post();
                    for (auto *provider : ImHexApi::Provider::getProviders())
                        ImHexApi::Provider::remove(provider, true);
                    ProjectManager::loadTemporaryProject();
                });
            }
        };

        // The menu and provider registration both require foreground tasks to finish.
        bool waitForProjectTasks(ImGuiTestContext *ctx) {
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
            do {
                ctx->Yield();
                if (TaskManager::getRunningTaskCount() == 0)
                    return !ctx->IsError();
            } while (!ctx->IsError() && std::chrono::steady_clock::now() < deadline);
            return false;
        }

        // Close through the nested File menu and wait until reopening is possible.
        bool closeProjectFromMenu(ImGuiTestContext *ctx) {
            if (!waitForProjectTasks(ctx))
                return false;

            ctx->MenuClick(fmt::format("//##MainMenuBar/{}/{}/{}",
                "hex.builtin.menu.file"_lang, "hex.builtin.menu.file.project"_lang,
                "hex.builtin.menu.file.project.close"_lang).c_str());
            return waitForProjectTasks(ctx) && ProjectManager::isTemporaryProject() &&
                ImHexApi::Provider::getProviders().empty();
        }

        // Click a byte in the main editor rather than changing the selection through the API.
        void clickByte(ImGuiTestContext *ctx, const char *item) {
            ctx->SetRef("###hex.builtin.view.hex_editor.name");
            ctx->ItemClick(item, ImGuiMouseButton_Left, ImGuiTestOpFlags_NoCheckHoveredId);
        }

    }

    // A saved project restores the source file, not merely an empty project.
    IMGUI_TEST_SEQUENCE("Project", "Save and reopen a project", ctx) {
        IM_CHECK(ImHexApi::Provider::getProviders().empty());
        const TestFiles files;
        const std::vector<u8> bytes = { 0x11, 0x22, 0x33 };
        const auto filePath = files.create("source.bin", bytes);
        IM_CHECK(openFileByInjectedDrop(ctx, filePath) != nullptr);
        const ProjectCleanup project { files.directory() / "project" };

        IM_CHECK(ProjectManager::store(project.path));
        IM_CHECK(closeProjectFromMenu(ctx));
        IM_CHECK(ProjectManager::load(project.path));

        IM_CHECK(ImHexApi::Provider::getProviders().size() == 1);
        auto *restored = ImHexApi::Provider::get();
        const auto *file = dynamic_cast<prv::IProviderFilePicker *>(restored);
        IM_CHECK(file != nullptr && file->getPickedPath() == filePath);
        IM_CHECK(hasBytes(*restored, bytes));
        IM_CHECK(closeProjectFromMenu(ctx));
    };

    // The restored bookmark must select its original range when used from the Bookmarks view.
    IMGUI_TEST_SEQUENCE("Project", "Restore a bookmark when reopening a project", ctx) {
        IM_CHECK(ImHexApi::Provider::getProviders().empty());
        const TestFiles files;
        const auto filePath = files.create("source.bin", { 0x11, 0x22, 0x33 });
        IM_CHECK(openFileByInjectedDrop(ctx, filePath) != nullptr);
        const ProjectCleanup project { files.directory() / "project" };

        clickByte(ctx, "**/hex-byte-1");
        ctx->KeyDown(ImGuiMod_Shift);
        clickByte(ctx, "**/hex-byte-2");
        ctx->KeyUp(ImGuiMod_Shift);
        clickMenu(ctx, "hex.builtin.menu.edit"_lang, "hex.builtin.menu.edit.bookmark.create"_lang);
        IM_CHECK(!ctx->IsError());

        IM_CHECK(ProjectManager::store(project.path));
        IM_CHECK(closeProjectFromMenu(ctx));
        IM_CHECK(ProjectManager::load(project.path));

        clickByte(ctx, "**/hex-byte-0");
        const auto before = ImHexApi::HexEditor::getSelection();
        IM_CHECK(before.has_value() && before->address == 0 && before->size == 1);

        auto *bookmarks = ContentRegistry::Views::getViewByName("hex.builtin.view.bookmarks.name"_unlocalized);
        IM_CHECK(bookmarks != nullptr);
        const bool wasOpen = bookmarks->getWindowOpenState();
        auto restoreView = SCOPE_GUARD { bookmarks->getWindowOpenState() = wasOpen; };
        bookmarks->getWindowOpenState() = true;
        ctx->Yield(3);
        ctx->WindowFocus("//###hex.builtin.view.bookmarks.name");
        ctx->Yield();
        ctx->SetRef("###hex.builtin.view.bookmarks.name");
        ctx->ItemClick("**/" ICON_VS_DEBUG_STEP_BACK);

        const auto selection = ImHexApi::HexEditor::getSelection();
        IM_CHECK(selection.has_value() && selection->address == 1 && selection->size == 2);
        IM_CHECK(closeProjectFromMenu(ctx));
    };

}
