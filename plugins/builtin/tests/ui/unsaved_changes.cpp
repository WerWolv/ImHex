#include "ui_test_helpers.hpp"

#include <hex/api/content_registry/views.hpp>
#include <hex/providers/file_backed_provider_data.hpp>

#include <fonts/vscode_icons.hpp>
#include <imgui_internal.h>

namespace hex::plugin::builtin {

    using namespace ui_test;

    namespace {
        const std::vector<u8> Original = { 0x11, 0x22, 0x33 };
        const std::vector<u8> Edited = { 0x11, 0x22, 0xA5 };

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

        // The code editor is a custom child window, not an ImGui input widget.
        void editPattern(ImGuiTestContext *ctx) {
            auto *view = ContentRegistry::Views::getViewByName("hex.builtin.view.pattern_editor.name"_unlocalized);
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
            ctx->KeyChars("u8 value @ 0;");
        }

    }

    // Cancel leaves the dirty provider open without saving its edit.
    IMGUI_TEST_SEQUENCE("Unsaved Changes", "Cancel unsaved file close", ctx) {
        IM_CHECK(ImHexApi::Provider::getProviders().empty());
        const TestFiles files;
        const auto path = files.create("cancel.bin", Original);
        auto *provider = openFileByInjectedDrop(ctx, path);
        IM_CHECK(provider != nullptr);
        editByte(ctx, *provider);

        closeFile(ctx);
        IM_CHECK(setUnsavedPopupRef(ctx));
        checkPrompt(ctx, true, false);
        ctx->ItemClick(fmt::format("**/{}", "hex.ui.common.cancel"_lang).c_str());
        ctx->Yield(2);
        IM_CHECK(getUnsavedPopup(ctx) == nullptr);
        IM_CHECK(ImHexApi::Provider::get() == provider);
        IM_CHECK(provider->isDataDirty());
        IM_CHECK(hasBytes(*provider, Edited));
        IM_CHECK(diskHasBytes(path, Original));
    };

    // Discard closes the provider without writing the edit.
    IMGUI_TEST_SEQUENCE("Unsaved Changes", "Discard unsaved file changes", ctx) {
        IM_CHECK(ImHexApi::Provider::getProviders().empty());
        const TestFiles files;
        const auto path = files.create("discard.bin", Original);
        auto *provider = openFileByInjectedDrop(ctx, path);
        IM_CHECK(provider != nullptr);
        editByte(ctx, *provider);

        closeFile(ctx);
        IM_CHECK(setUnsavedPopupRef(ctx));
        checkPrompt(ctx, true, false);
        ctx->ItemClick(fmt::format("**/{}", "hex.ui.common.discard"_lang).c_str());
        IM_CHECK(ImHexApi::Provider::getProviders().empty());
        IM_CHECK(diskHasBytes(path, Original));
    };

    // Saving through the close prompt persists the edited data before closing.
    IMGUI_TEST_SEQUENCE("Unsaved Changes", "Save data on unsaved file close", ctx) {
        IM_CHECK(ImHexApi::Provider::getProviders().empty());
        const TestFiles files;
        const auto path = files.create("save.bin", Original);
        auto *provider = openFileByInjectedDrop(ctx, path);
        IM_CHECK(provider != nullptr);
        editByte(ctx, *provider);

        closeFile(ctx);
        IM_CHECK(setUnsavedPopupRef(ctx));
        checkPrompt(ctx, true, false);
        ctx->ItemClick(fmt::format("**/{}", "hex.ui.common.save.data_only"_lang).c_str());
        IM_CHECK(ImHexApi::Provider::getProviders().empty());
        IM_CHECK(diskHasBytes(path, Edited));
    };

    // Folder projects normally flush pattern edits rather than showing a close prompt.
    IMGUI_TEST_SEQUENCE("Unsaved Changes", "Warn before closing when a pattern cannot be saved", ctx) {
        IM_CHECK(ImHexApi::Provider::getProviders().empty());
        const TestFiles files;
        const auto path = files.create("pattern.bin", Original);
        const auto patternPath = files.create("pattern.hexpat", {});
        IM_CHECK(!patternPath.empty());
        auto *provider = openFileByInjectedDrop(ctx, path);
        IM_CHECK(provider != nullptr);

        auto *pattern = FileBackedProviderDataRegistry::get("hex.builtin.pattern-source");
        IM_CHECK(pattern != nullptr);
        TaskManager::doLater([patternPath] { EventFileDropped::post(patternPath); });
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        do {
            ctx->Yield();
        } while (!ctx->IsError() && std::chrono::steady_clock::now() < deadline &&
                (pattern->getBinding(provider) != patternPath || TaskManager::getRunningTaskCount() != 0));
        IM_CHECK(pattern->getBinding(provider) == patternPath);
        IM_CHECK(TaskManager::getRunningTaskCount() == 0);
        IM_CHECK(!pattern->hasPendingData(provider));

        editPattern(ctx);
        ctx->Yield();
        IM_CHECK(pattern->hasPendingData(provider));
        IM_CHECK(!provider->isDataDirty() && !provider->isMetadataDirty());

        // Keep the editor dirty while making the atomic replacement fail.
        std::error_code error;
        std::fs::rename(patternPath, files.directory() / "original.hexpat", error);
        IM_CHECK(!error);
        IM_CHECK(std::fs::create_directory(patternPath));

        closeFile(ctx);
        IM_CHECK(setUnsavedPopupRef(ctx));
        checkPrompt(ctx, false, true);
        ctx->ItemClick(fmt::format("**/{}", "hex.ui.common.cancel"_lang).c_str());
        IM_CHECK(getUnsavedPopup(ctx) == nullptr);
        IM_CHECK(ImHexApi::Provider::get() == provider);
        IM_CHECK(pattern->hasPendingData(provider));
        IM_CHECK(diskHasBytes(path, Original));
    };

}
