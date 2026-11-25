#include "ui_test_helpers.hpp"

#if defined(OS_LINUX) || defined(OS_MACOS) || defined(OS_FREEBSD)

#include <unistd.h>

namespace hex::plugin::builtin {

    using namespace ui_test;

    IMGUI_TEST_SEQUENCE("Editing", "Read-only file cannot be edited", ctx) {
        // Root can bypass file permission bits.
        if (geteuid() == 0)
            return;

        // Remove write permission before opening the file.
        IM_CHECK(ImHexApi::Provider::getProviders().empty());
        const TestFiles files;
        const std::vector<u8> original = { 0x11, 0x22, 0x33 };
        const auto path = files.create("read-only.bin", original);
        IM_CHECK(!path.empty());
        std::fs::permissions(path, std::fs::perms::owner_read, std::fs::perm_options::replace);
        auto *provider = openFileByInjectedDrop(ctx, path);
        IM_CHECK(provider != nullptr);
        IM_CHECK(provider->isReadable() && !provider->isWritable() && !provider->isResizable());

        // Typing over a byte should neither change it nor mark the file dirty.
        ctx->SetRef("###hex.builtin.view.hex_editor.name");
        ctx->ItemDoubleClick("**/hex-byte-1", ImGuiTestOpFlags_NoCheckHoveredId);
        ctx->KeyChars("A5");
        IM_CHECK(!provider->isDataDirty());
        IM_CHECK(hasBytes(*provider, original));

        // Commands that modify the file should also be disabled.
        const auto editMenu = fmt::format("//##MainMenuBar/{}", "hex.builtin.menu.edit"_lang);
        ctx->MenuAction(ImGuiTestAction_Open, editMenu.c_str());
        for (const auto *item : { "hex.builtin.view.hex_editor.menu.edit.insert",
                                  "hex.builtin.view.hex_editor.menu.edit.remove",
                                  "hex.builtin.view.hex_editor.menu.edit.fill" }) {
            const auto itemPath = fmt::format("//$FOCUSED/{}", Lang(item).get());
            IM_CHECK((ctx->ItemInfo(itemPath.c_str()).ItemFlags & ImGuiItemFlags_Disabled) != 0);
        }
        ctx->KeyPress(ImGuiKey_Escape);

        // Closing should leave the original bytes on disk.
        closeFile(ctx);
        IM_CHECK(ImHexApi::Provider::getProviders().empty());
        wolv::io::File saved(path, wolv::io::File::Mode::Read);
        IM_CHECK(saved.isValid());
        IM_CHECK(saved.readVector() == original);
    };

}

#endif
