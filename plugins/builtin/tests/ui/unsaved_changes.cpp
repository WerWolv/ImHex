#include "ui_test_helpers.hpp"

#include <content/views/view_pattern_editor.hpp>
#include <hex/api/content_registry/views.hpp>
#include <hex/providers/file_backed_provider_data.hpp>

#include <fonts/vscode_icons.hpp>
#include <imgui_internal.h>
#include <wolv/utils/string.hpp>

namespace hex::plugin::builtin {

    using namespace ui_test;

    namespace {
        // Shared baseline and expected result for the file edits.
        const std::vector<u8> Original = { 0x11, 0x22, 0x33 };
        const std::vector<u8> Edited = { 0x11, 0x22, 0xA5 };
        constexpr auto PatternSource = "u8 value @ 0;";

        // Produce and verify the same unsaved hex edit in each data-only test.
        void editByte(ImGuiTestContext *ctx, prv::Provider &provider) {
            IM_CHECK(provider.isWritable() && !provider.isDataDirty());
            ctx->SetRef("###hex.builtin.view.hex_editor.name");
            ctx->ItemDoubleClick("**/hex-byte-2", ImGuiTestOpFlags_NoCheckHoveredId);
            ctx->KeyChars("A5");
            IM_CHECK(provider.isDataDirty());
            IM_CHECK(hasBytes(provider, Edited));
        }

        // Use the open popup ID to find the current question dialog.
        ImGuiWindow *getUnsavedPopup(ImGuiTestContext *ctx) {
            const auto name = fmt::format("{} {}", ICON_VS_SAVE_AS, "hex.ui.common.question"_lang);
            for (const auto &popup : ctx->UiContext->OpenPopupStack) {
                if (popup.PopupId == ImHashStr(name.c_str(), 0, popup.OpenParentId) &&
                    popup.Window != nullptr && popup.Window->Active)
                    return popup.Window;
            }
            return nullptr;
        }

        // Wait until the queued popup has drawn its buttons.
        bool setUnsavedPopupRef(ImGuiTestContext *ctx) {
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
            do {
                ctx->Yield();
                if (auto *popup = getUnsavedPopup(ctx); popup != nullptr) {
                    ctx->SetRef(popup);
                    return true;
                }
            } while (!ctx->IsError() && std::chrono::steady_clock::now() < deadline);
            return false;
        }

        // Save choices depend on what changed; Cancel and Discard are always available.
        void checkPrompt(ImGuiTestContext *ctx, bool dataDirty, bool metadataDirty) {
            const auto checkButton = [ctx](const char *label, bool enabled) {
                const auto item = ctx->ItemInfo(fmt::format("**/{}", label).c_str());
                IM_CHECK(item.ID != 0);
                IM_CHECK(((item.ItemFlags & ImGuiItemFlags_Disabled) == 0) == enabled);
            };
            checkButton("hex.ui.common.save.data_only"_lang, dataDirty);
            checkButton("hex.ui.common.save.project"_lang, metadataDirty);
            checkButton("hex.ui.common.cancel"_lang, true);
            checkButton("hex.ui.common.discard"_lang, true);
        }

        // Read through a separate handle so unsaved provider edits cannot satisfy the check.
        bool diskHasBytes(const std::fs::path &path, const std::vector<u8> &expected) {
            wolv::io::File file(path, wolv::io::File::Mode::Read);
            return file.isValid() && file.readVector() == expected;
        }

        // Load a backing pattern through the file drop handler and wait for it to bind.
        bool openPattern(ImGuiTestContext *ctx, const prv::Provider *provider,
                         const FileBackedProviderDataBase &pattern, const std::fs::path &path) {
            TaskManager::doLater([path] { EventFileDropped::post(path); });
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
            do {
                ctx->Yield();
                if (pattern.getBinding(provider) == path && TaskManager::getRunningTaskCount() == 0)
                    return !ctx->IsError();
            } while (!ctx->IsError() && std::chrono::steady_clock::now() < deadline);
            return false;
        }

        // Read the editor for the current provider, not the pattern backing file.
        ViewPatternEditor *getPatternEditor() {
            return static_cast<ViewPatternEditor *>(ContentRegistry::Views::getViewByName("hex.builtin.view.pattern_editor.name"_unlocalized));
        }

        // The code editor is a custom child window, not an ImGui input widget.
        void editPattern(ImGuiTestContext *ctx) {
            auto *view = getPatternEditor();
            IM_CHECK(view != nullptr);
            if (!view->getWindowOpenState())
                clickMenu(ctx, "hex.builtin.menu.view"_lang, "hex.builtin.view.pattern_editor.name"_lang);

            constexpr auto Window = "###hex.builtin.view.pattern_editor.name";
            ctx->WindowFocus("//###hex.builtin.view.pattern_editor.name");
            // The pattern view skips drawing its editor on the first frame after appearing.
            ctx->Yield(3);
            ctx->SetRef(Window);
            const auto editor = ctx->WindowInfo("##pattern_editor_resizer/##pattern_editor");
            IM_CHECK(editor.ID != 0);
            ctx->MouseMoveToPos(editor.RectClipped.GetCenter());
            ctx->MouseClick();
            ctx->KeyChars(PatternSource);
        }

    }

    IMGUI_TEST_SEQUENCE("Unsaved Changes", "Cancel unsaved file close", ctx) {
        // Leave one unsaved byte edit in a fresh file.
        IM_CHECK(ImHexApi::Provider::getProviders().empty());
        const TestFiles files;
        const auto path = files.create("cancel.bin", Original);
        auto *provider = openFileByInjectedDrop(ctx, path);
        IM_CHECK(provider != nullptr);
        editByte(ctx, *provider);

        // Cancel the close from the data-only prompt.
        closeFile(ctx);
        IM_CHECK(setUnsavedPopupRef(ctx));
        checkPrompt(ctx, true, false);
        ctx->ItemClick(fmt::format("**/{}", "hex.ui.common.cancel"_lang).c_str());
        ctx->Yield(2);

        // Verify the edit stays in memory but not on disk.
        IM_CHECK(getUnsavedPopup(ctx) == nullptr);
        IM_CHECK(ImHexApi::Provider::get() == provider);
        IM_CHECK(provider->isDataDirty());
        IM_CHECK(hasBytes(*provider, Edited));
        IM_CHECK(diskHasBytes(path, Original));
    };

    IMGUI_TEST_SEQUENCE("Unsaved Changes", "Discard unsaved file changes", ctx) {
        // Leave one unsaved byte edit in a fresh file.
        IM_CHECK(ImHexApi::Provider::getProviders().empty());
        const TestFiles files;
        const auto path = files.create("discard.bin", Original);
        auto *provider = openFileByInjectedDrop(ctx, path);
        IM_CHECK(provider != nullptr);
        editByte(ctx, *provider);

        // Discard the edit from the data-only prompt.
        closeFile(ctx);
        IM_CHECK(setUnsavedPopupRef(ctx));
        checkPrompt(ctx, true, false);
        ctx->ItemClick(fmt::format("**/{}", "hex.ui.common.discard"_lang).c_str());

        // Verify the file closes without changing the disk bytes.
        IM_CHECK(ImHexApi::Provider::getProviders().empty());
        IM_CHECK(diskHasBytes(path, Original));
    };

    IMGUI_TEST_SEQUENCE("Unsaved Changes", "Save data on unsaved file close", ctx) {
        // Leave one unsaved byte edit in a fresh file.
        IM_CHECK(ImHexApi::Provider::getProviders().empty());
        const TestFiles files;
        const auto path = files.create("save.bin", Original);
        auto *provider = openFileByInjectedDrop(ctx, path);
        IM_CHECK(provider != nullptr);
        editByte(ctx, *provider);

        // Save only the data from the close prompt.
        closeFile(ctx);
        IM_CHECK(setUnsavedPopupRef(ctx));
        checkPrompt(ctx, true, false);
        ctx->ItemClick(fmt::format("**/{}", "hex.ui.common.save.data_only"_lang).c_str());

        // Verify the file closes and the edit reaches disk.
        IM_CHECK(ImHexApi::Provider::getProviders().empty());
        IM_CHECK(diskHasBytes(path, Edited));
    };

    IMGUI_TEST_SEQUENCE("Unsaved Changes", "Warn before closing when a pattern cannot be saved", ctx) {
        // Create separate data and pattern files for this provider.
        IM_CHECK(ImHexApi::Provider::getProviders().empty());
        const TestFiles files;
        const auto path = files.create("pattern.bin", Original);
        const auto patternPath = files.create("pattern.hexpat", {});
        IM_CHECK(!patternPath.empty());
        auto *provider = openFileByInjectedDrop(ctx, path);
        IM_CHECK(provider != nullptr);

        // Wait for the dropped pattern to bind as a clean backing file.
        auto *pattern = FileBackedProviderDataRegistry::get("hex.builtin.pattern-source");
        IM_CHECK(pattern != nullptr);
        IM_CHECK(openPattern(ctx, provider, *pattern, patternPath));
        IM_CHECK(!pattern->hasPendingData(provider));

        // Leave only a pending pattern edit, not a provider edit.
        editPattern(ctx);
        ctx->Yield();
        IM_CHECK(pattern->hasPendingData(provider));
        IM_CHECK(!provider->isDataDirty() && !provider->isMetadataDirty());

        // Keep the editor dirty while making the atomic replacement fail.
        std::error_code error;
        std::fs::rename(patternPath, files.directory() / "original.hexpat", error);
        IM_CHECK(!error);
        IM_CHECK(std::fs::create_directory(patternPath));

        // Cancel the close from the project-save prompt.
        closeFile(ctx);
        IM_CHECK(setUnsavedPopupRef(ctx));
        checkPrompt(ctx, false, true);
        ctx->ItemClick(fmt::format("**/{}", "hex.ui.common.cancel"_lang).c_str());

        // Verify the pattern edit survives and the data file stays unchanged.
        IM_CHECK(getUnsavedPopup(ctx) == nullptr);
        IM_CHECK(ImHexApi::Provider::get() == provider);
        IM_CHECK(pattern->hasPendingData(provider));
        IM_CHECK(diskHasBytes(path, Original));
    };

    IMGUI_TEST_SEQUENCE("Unsaved Changes", "Keep edits when Save Data Only fails on close", ctx) {
        // Replace the backing file with a directory after a real in-memory edit.
        IM_CHECK(ImHexApi::Provider::getProviders().empty());
        const TestFiles files;
        const auto path = files.create("save-fails.bin", Original);
        auto *provider = openFileByInjectedDrop(ctx, path);
        IM_CHECK(provider != nullptr);
        editByte(ctx, *provider);
        const auto originalPath = files.directory() / "original.bin";
        std::fs::rename(path, originalPath);
        IM_CHECK(std::fs::create_directory(path));

        // A failed save must abort the close without discarding the edit.
        closeFile(ctx);
        IM_CHECK(setUnsavedPopupRef(ctx));
        ctx->ItemClick(fmt::format("**/{}", "hex.ui.common.save.data_only"_lang).c_str());
        ctx->Yield(2);
        IM_CHECK(ImHexApi::Provider::get() == provider);
        IM_CHECK(provider->isDataDirty() && hasBytes(*provider, Edited));
        IM_CHECK(diskHasBytes(originalPath, Original));

        // Restore the file and verify the same edit can still be saved.
        IM_CHECK(std::fs::remove(path));
        std::fs::rename(originalPath, path);
        closeFile(ctx);
        IM_CHECK(setUnsavedPopupRef(ctx));
        ctx->ItemClick(fmt::format("**/{}", "hex.ui.common.save.data_only"_lang).c_str());
        IM_CHECK(ImHexApi::Provider::getProviders().empty());
        IM_CHECK(diskHasBytes(path, Edited));
    };

    IMGUI_TEST_SEQUENCE("Unsaved Changes", "Keep edits when Save As fails", ctx) {
        const TestFiles files;
        const auto path = files.create("source.bin", Original);
        auto *provider = openFileByInjectedDrop(ctx, path);
        IM_CHECK(provider != nullptr);
        editByte(ctx, *provider);

        // Use a directory as the destination to force an open failure without a native dialog.
        const auto destination = files.directory() / "export.bin";
        IM_CHECK(std::fs::create_directory(destination));
        const auto failure = provider->saveAs(destination);
        IM_CHECK(!failure.has_value());
        IM_CHECK(failure.error().find("export.bin") != std::string::npos);
        IM_CHECK(provider->isDataDirty() && hasBytes(*provider, Edited));

        IM_CHECK(std::fs::remove(destination));
        IM_CHECK(provider->saveAs(destination).has_value());
        IM_CHECK(!provider->isDataDirty());
        IM_CHECK(diskHasBytes(destination, Edited) && diskHasBytes(path, Original));
        closeFile(ctx);
        IM_CHECK(ImHexApi::Provider::getProviders().empty());
    };

    IMGUI_TEST_SEQUENCE("Unsaved Changes", "Restore a pattern after closing and reopening its file", ctx) {
        // Bind an empty pattern file and type a declaration in the editor.
        IM_CHECK(ImHexApi::Provider::getProviders().empty());
        const TestFiles files;
        const auto path = files.create("pattern.bin", Original);
        const auto patternPath = files.create("pattern.hexpat", {});
        IM_CHECK(!patternPath.empty());
        auto *provider = openFileByInjectedDrop(ctx, path);
        auto *pattern = FileBackedProviderDataRegistry::get("hex.builtin.pattern-source");
        IM_CHECK(provider != nullptr && pattern != nullptr);
        IM_CHECK(openPattern(ctx, provider, *pattern, patternPath));
        editPattern(ctx);
        ctx->Yield();
        IM_CHECK(pattern->hasPendingData(provider));

        // Close normally and verify the edited text reached the backing file.
        closeFile(ctx);
        IM_CHECK(ImHexApi::Provider::getProviders().empty());
        {
            wolv::io::File saved(patternPath, wolv::io::File::Mode::Read);
            IM_CHECK(saved.isValid() && wolv::util::trim(saved.readString()) == PatternSource);
        }

        // Reopen the data and pattern through the same handlers and inspect the new editor.
        auto *reopened = openFileByInjectedDrop(ctx, path);
        IM_CHECK(reopened != nullptr);
        IM_CHECK(openPattern(ctx, reopened, *pattern, patternPath));
        auto *view = getPatternEditor();
        IM_CHECK(view != nullptr && wolv::util::trim(view->getTextEditor()->getText()) == PatternSource);
        IM_CHECK(!pattern->hasPendingData(reopened));
        closeFile(ctx);
        IM_CHECK(ImHexApi::Provider::getProviders().empty());
    };

    IMGUI_TEST_SEQUENCE("Unsaved Changes", "Cancel closing a project with unsaved byte edits", ctx) {
        // Load a fresh project directly so setup cannot promote the default project.
        IM_CHECK(ImHexApi::Provider::getProviders().empty());
        const TestFiles files;
        IM_CHECK(!files.directory().empty());
        const ProjectCleanup project { files.directory() };
        IM_CHECK(ProjectManager::load(project.path));
        const auto path = files.create("project-data.bin", Original);
        auto *provider = openFileByInjectedDrop(ctx, path);
        IM_CHECK(provider != nullptr);
        IM_CHECK(ProjectManager::store());
        editByte(ctx, *provider);

        // Cancel Close Project, not Close File.
        requestProjectClose(ctx);
        IM_CHECK(setUnsavedPopupRef(ctx));
        ctx->ItemClick(fmt::format("**/{}", "hex.ui.common.cancel"_lang).c_str());
        ctx->Yield(2);

        // The same project and dirty provider should remain open.
        IM_CHECK(getUnsavedPopup(ctx) == nullptr);
        IM_CHECK(ProjectManager::getPath() == project.path && !ProjectManager::isTemporaryProject());
        IM_CHECK(ImHexApi::Provider::get() == provider);
        IM_CHECK(provider->isDataDirty() && hasBytes(*provider, Edited));
        IM_CHECK(diskHasBytes(path, Original));
    };

}
