#include "ui_test_helpers.hpp"

#include <fonts/vscode_icons.hpp>
#include <hex/api/content_registry/views.hpp>
#include <hex/api/imhex_api/hex_editor.hpp>

#include <chrono>

namespace hex::plugin::builtin {

    using namespace ui_test;

    namespace {

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
            // Wait for the menu to become available.
            if (!waitForProjectTasks(ctx))
                return false;

            // Closing must leave a temporary project with no providers.
            requestProjectClose(ctx);
            return waitForProjectTasks(ctx) && ProjectManager::isTemporaryProject() &&
                ImHexApi::Provider::getProviders().empty();
        }

        // Click a byte in the main editor rather than changing the selection through the API.
        void clickByte(ImGuiTestContext *ctx, const char *item) {
            ctx->SetRef("###hex.builtin.view.hex_editor.name");
            ctx->ItemClick(item, ImGuiMouseButton_Left, ImGuiTestOpFlags_NoCheckHoveredId);
        }

    }

    IMGUI_TEST_SEQUENCE("Project", "Save and reopen a project", ctx) {
        // Start with a project backed by a known source file.
        IM_CHECK(ImHexApi::Provider::getProviders().empty());
        const TestFiles files;
        const std::vector<u8> bytes = { 0x11, 0x22, 0x33 };
        const auto filePath = files.create("source.bin", bytes);
        IM_CHECK(openFileByInjectedDrop(ctx, filePath) != nullptr);
        const ProjectCleanup project { files.directory() / "project" };

        // Close the saved project before loading it to force a fresh provider.
        IM_CHECK(ProjectManager::store(project.path));
        IM_CHECK(closeProjectFromMenu(ctx));
        IM_CHECK(ProjectManager::load(project.path));

        // Check that the reopened provider has the original path and bytes.
        IM_CHECK(ImHexApi::Provider::getProviders().size() == 1);
        auto *restored = ImHexApi::Provider::get();
        const auto *file = dynamic_cast<prv::IProviderFilePicker *>(restored);
        IM_CHECK(file != nullptr && file->getPickedPath() == filePath);
        IM_CHECK(hasBytes(*restored, bytes));
        IM_CHECK(closeProjectFromMenu(ctx));
    };

    IMGUI_TEST_SEQUENCE("Project", "Restore a bookmark when reopening a project", ctx) {
        // Start with a clean project and a three-byte source file.
        IM_CHECK(ImHexApi::Provider::getProviders().empty());
        const TestFiles files;
        const auto filePath = files.create("source.bin", { 0x11, 0x22, 0x33 });
        IM_CHECK(openFileByInjectedDrop(ctx, filePath) != nullptr);
        const ProjectCleanup project { files.directory() / "project" };

        // Bookmark a two-byte range through the Edit menu.
        clickByte(ctx, "**/hex-byte-1");
        ctx->KeyDown(ImGuiMod_Shift);
        clickByte(ctx, "**/hex-byte-2");
        ctx->KeyUp(ImGuiMod_Shift);
        clickMenu(ctx, "hex.builtin.menu.edit"_lang, "hex.builtin.menu.edit.bookmark.create"_lang);
        IM_CHECK(!ctx->IsError());

        // Reopen the saved project to exercise bookmark persistence.
        IM_CHECK(ProjectManager::store(project.path));
        IM_CHECK(closeProjectFromMenu(ctx));
        IM_CHECK(ProjectManager::load(project.path));

        // Move off the bookmarked range so navigation must change the selection.
        clickByte(ctx, "**/hex-byte-0");
        const auto before = ImHexApi::HexEditor::getSelection();
        IM_CHECK(before.has_value() && before->address == 0 && before->size == 1);

        // Use the restored bookmark's jump action and preserve the view's previous state.
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

        // The jump should restore the original two-byte range.
        const auto selection = ImHexApi::HexEditor::getSelection();
        IM_CHECK(selection.has_value() && selection->address == 1 && selection->size == 2);
        IM_CHECK(closeProjectFromMenu(ctx));
    };

}
