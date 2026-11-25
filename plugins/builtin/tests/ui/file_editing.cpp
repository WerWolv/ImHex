#include "ui_test_helpers.hpp"

namespace hex::plugin::builtin {

    using namespace ui_test;

    IMGUI_TEST_SEQUENCE("Hex Editor (injected file setup)", "Overwrite, save and close a file", ctx) {
        IM_CHECK(ImHexApi::Provider::getProviders().empty());

        // Open a writable file with a known last byte.
        const TestFiles files;
        const auto path = files.create("edit.bin", { 0x11, 0x22, 0x33 });
        IM_CHECK(!path.empty());
        auto *provider = openFileByInjectedDrop(ctx, path);
        IM_CHECK(provider != nullptr && provider->isWritable());
        IM_CHECK(provider->getActualSize() == 3);

        // Overwrite the last byte and check that the edit is unsaved.
        ctx->SetRef("###hex.builtin.view.hex_editor.name");
        ctx->ItemDoubleClick("**/hex-byte-2", ImGuiTestOpFlags_NoCheckHoveredId);
        ctx->KeyChars("A5");
        IM_CHECK(provider->isDataDirty());

        // Save explicitly so closing should not need a prompt.
        clickMenu(ctx, "hex.builtin.menu.file"_lang, "hex.builtin.view.hex_editor.menu.file.save"_lang);
        IM_CHECK(!provider->isDataDirty());

        // Verify that the saved file closes.
        closeFile(ctx);
        IM_CHECK(ImHexApi::Provider::getProviders().empty());

        // Read the file independently of the editor buffer.
        wolv::io::File saved(path, wolv::io::File::Mode::Read);
        const std::vector<u8> expected = { 0x11, 0x22, 0xA5 };
        IM_CHECK(saved.isValid());
        IM_CHECK(saved.readVector() == expected);
    };

    IMGUI_TEST_SEQUENCE("Hex Editor (injected file setup)", "Switch between two files and close them", ctx) {
        IM_CHECK(ImHexApi::Provider::getProviders().empty());

        // Open two distinguishable files; the newest tab should be selected.
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

        // Switch back to the first file through its tab.
        const auto firstTab = fmt::format("//ImHexDockSpace/**/{} {}", first->getIcon(), first->getName());
        ctx->ItemClick(firstTab.c_str());
        ctx->Yield();
        IM_CHECK(ImHexApi::Provider::get() == first);

        // Closing the first tab should return to the intact second file.
        closeFile(ctx);
        IM_CHECK(ImHexApi::Provider::getProviders().size() == 1);
        IM_CHECK(ImHexApi::Provider::get() == second);
        IM_CHECK(hasBytes(*second, { 0x22 }));

        // Close the remaining tab.
        closeFile(ctx);
        IM_CHECK(ImHexApi::Provider::getProviders().empty());
    };

    IMGUI_TEST_SEQUENCE("Hex Editor (injected file setup)", "Insert bytes, undo and redo", ctx) {
        IM_CHECK(ImHexApi::Provider::getProviders().empty());

        // Open a file with distinct bytes so shifts are visible.
        const TestFiles files;
        const std::vector<u8> original = { 0x11, 0x22, 0x33 };
        const auto path = files.create("insert.bin", original);
        IM_CHECK(!path.empty());
        auto *provider = openFileByInjectedDrop(ctx, path);
        IM_CHECK(provider != nullptr && provider->isResizable());

        // Use the middle byte as the insertion point.
        ctx->SetRef("###hex.builtin.view.hex_editor.name");
        ctx->ItemClick("**/hex-byte-1", ImGuiMouseButton_Left, ImGuiTestOpFlags_NoCheckHoveredId);
        clickMenu(ctx, "hex.builtin.menu.edit"_lang, "hex.builtin.view.hex_editor.menu.edit.insert"_lang);

        // Request a two-byte insert.
        ctx->SetRef("##hex.builtin.view.hex_editor.menu.edit.insert");
        ctx->ItemClick("hex.ui.common.size"_lang.get());
        ctx->KeyCharsReplace("2");
        ctx->ItemClick("hex.ui.common.set"_lang.get());

        // Both inserted bytes should be zero and the original suffix should shift.
        const std::vector<u8> inserted = { 0x11, 0x00, 0x00, 0x22, 0x33 };
        IM_CHECK(hasBytes(*provider, inserted));

        // Undo should restore the original length and bytes.
        clickMenu(ctx, "hex.builtin.menu.edit"_lang, "hex.builtin.view.hex_editor.menu.edit.undo"_lang);
        IM_CHECK(hasBytes(*provider, original));

        // Redo should restore the insertion.
        clickMenu(ctx, "hex.builtin.menu.edit"_lang, "hex.builtin.view.hex_editor.menu.edit.redo"_lang);
        IM_CHECK(hasBytes(*provider, inserted));

        // Verify that the redone insertion survives Save and Close.
        clickMenu(ctx, "hex.builtin.menu.file"_lang, "hex.builtin.view.hex_editor.menu.file.save"_lang);
        closeFile(ctx);
        IM_CHECK(ImHexApi::Provider::getProviders().empty());
        wolv::io::File saved(path, wolv::io::File::Mode::Read);
        IM_CHECK(saved.isValid());
        IM_CHECK(saved.readVector() == inserted);
    };

    IMGUI_TEST_SEQUENCE("Hex Editor (injected file setup)", "Remove a byte, undo and redo", ctx) {
        // Open a file with a distinct byte to remove between two neighbors.
        IM_CHECK(ImHexApi::Provider::getProviders().empty());
        const TestFiles files;
        const std::vector<u8> original = { 0x11, 0x22, 0x33 };
        const auto path = files.create("remove.bin", original);
        IM_CHECK(!path.empty());
        auto *provider = openFileByInjectedDrop(ctx, path);
        IM_CHECK(provider != nullptr && provider->isResizable());

        // Remove the middle byte without altering its neighbors.
        ctx->SetRef("###hex.builtin.view.hex_editor.name");
        ctx->ItemClick("**/hex-byte-1", ImGuiMouseButton_Left, ImGuiTestOpFlags_NoCheckHoveredId);
        clickMenu(ctx, "hex.builtin.menu.edit"_lang, "hex.builtin.view.hex_editor.menu.edit.remove"_lang);
        ctx->SetRef("##hex.builtin.view.hex_editor.menu.edit.remove");
        ctx->ItemClick("hex.ui.common.set"_lang.get());
        IM_CHECK(hasBytes(*provider, { 0x11, 0x33 }));

        // Walk backward and forward through the removal.
        clickMenu(ctx, "hex.builtin.menu.edit"_lang, "hex.builtin.view.hex_editor.menu.edit.undo"_lang);
        IM_CHECK(hasBytes(*provider, original));
        clickMenu(ctx, "hex.builtin.menu.edit"_lang, "hex.builtin.view.hex_editor.menu.edit.redo"_lang);
        IM_CHECK(hasBytes(*provider, { 0x11, 0x33 }));

        // Verify that saving the smaller file leaves no trailing byte.
        clickMenu(ctx, "hex.builtin.menu.file"_lang, "hex.builtin.view.hex_editor.menu.file.save"_lang);
        closeFile(ctx);
        IM_CHECK(ImHexApi::Provider::getProviders().empty());
        wolv::io::File saved(path, wolv::io::File::Mode::Read);
        IM_CHECK(saved.isValid());
        IM_CHECK(saved.readVector() == std::vector<u8>({ 0x11, 0x33 }));
    };

    IMGUI_TEST_SEQUENCE("Hex Editor (injected file setup)", "Copy and paste a byte", ctx) {
        // Use distinct values so the overwritten byte is easy to identify.
        IM_CHECK(ImHexApi::Provider::getProviders().empty());
        const TestFiles files;
        const std::vector<u8> original = { 0x11, 0x22, 0x33 };
        const auto path = files.create("copy-paste.bin", original);
        IM_CHECK(!path.empty());
        auto *provider = openFileByInjectedDrop(ctx, path);
        IM_CHECK(provider != nullptr && provider->isWritable());

        // Copy the first byte and overwrite the last one with it.
        ctx->SetRef("###hex.builtin.view.hex_editor.name");
        ctx->ItemClick("**/hex-byte-0", ImGuiMouseButton_Left, ImGuiTestOpFlags_NoCheckHoveredId);
        clickMenu(ctx, "hex.builtin.menu.edit"_lang, "hex.builtin.view.hex_editor.menu.edit.copy"_lang);
        ctx->ItemClick("**/hex-byte-2", ImGuiMouseButton_Left, ImGuiTestOpFlags_NoCheckHoveredId);
        ctx->MenuClick(fmt::format("//##MainMenuBar/{}/{}/{}",
            "hex.builtin.menu.edit"_lang, "hex.builtin.view.hex_editor.menu.edit.paste_as"_lang,
            "hex.builtin.view.hex_editor.menu.edit.paste_all"_lang).c_str());
        IM_CHECK(hasBytes(*provider, { 0x11, 0x22, 0x11 }));

        // Restore the original bytes before saving and closing.
        clickMenu(ctx, "hex.builtin.menu.edit"_lang, "hex.builtin.view.hex_editor.menu.edit.undo"_lang);
        IM_CHECK(hasBytes(*provider, original));
        clickMenu(ctx, "hex.builtin.menu.file"_lang, "hex.builtin.view.hex_editor.menu.file.save"_lang);
        closeFile(ctx);
        IM_CHECK(ImHexApi::Provider::getProviders().empty());
    };

    IMGUI_TEST_SEQUENCE("Editing", "Insert bytes at the beginning, middle and end", ctx) {
        // Start with distinct bytes to track their positions after each insert.
        IM_CHECK(ImHexApi::Provider::getProviders().empty());
        const TestFiles files;
        const auto path = files.create("boundaries.bin", { 0x11, 0x22 });
        IM_CHECK(!path.empty());
        auto *provider = openFileByInjectedDrop(ctx, path);
        IM_CHECK(provider != nullptr && provider->isResizable());

        // Inserting at address zero should shift both original bytes.
        ctx->SetRef("###hex.builtin.view.hex_editor.name");
        ctx->ItemClick("**/hex-byte-0", ImGuiMouseButton_Left, ImGuiTestOpFlags_NoCheckHoveredId);
        clickMenu(ctx, "hex.builtin.menu.edit"_lang, "hex.builtin.view.hex_editor.menu.edit.insert"_lang);
        ctx->SetRef("##hex.builtin.view.hex_editor.menu.edit.insert");
        ctx->ItemClick("hex.ui.common.size"_lang.get());
        ctx->KeyCharsReplace("1");
        ctx->ItemClick("hex.ui.common.set"_lang.get());
        IM_CHECK(hasBytes(*provider, { 0x00, 0x11, 0x22 }));

        // Insert between the original bytes so only the suffix moves.
        ctx->SetRef("###hex.builtin.view.hex_editor.name");
        ctx->ItemClick("**/hex-byte-2", ImGuiMouseButton_Left, ImGuiTestOpFlags_NoCheckHoveredId);
        clickMenu(ctx, "hex.builtin.menu.edit"_lang, "hex.builtin.view.hex_editor.menu.edit.insert"_lang);
        ctx->SetRef("##hex.builtin.view.hex_editor.menu.edit.insert");
        ctx->ItemClick("hex.ui.common.size"_lang.get());
        ctx->KeyCharsReplace("2");
        ctx->ItemClick("hex.ui.common.set"_lang.get());
        IM_CHECK(hasBytes(*provider, { 0x00, 0x11, 0x00, 0x00, 0x22 }));

        // Append at the address immediately after the last byte.
        ctx->SetRef("###hex.builtin.view.hex_editor.name");
        ctx->ItemClick("**/hex-byte-4", ImGuiMouseButton_Left, ImGuiTestOpFlags_NoCheckHoveredId);
        clickMenu(ctx, "hex.builtin.menu.edit"_lang, "hex.builtin.view.hex_editor.menu.edit.insert"_lang);
        ctx->SetRef("##hex.builtin.view.hex_editor.menu.edit.insert");
        ctx->ItemClick("hex.ui.common.address"_lang.get());
        ctx->KeyCharsReplace("5");
        ctx->ItemClick("hex.ui.common.size"_lang.get());
        ctx->KeyCharsReplace("1");
        ctx->ItemClick("hex.ui.common.set"_lang.get());
        const std::vector<u8> expected = { 0x00, 0x11, 0x00, 0x00, 0x22, 0x00 };
        IM_CHECK(hasBytes(*provider, expected));

        // Verify all three insertions reached disk.
        clickMenu(ctx, "hex.builtin.menu.file"_lang, "hex.builtin.view.hex_editor.menu.file.save"_lang);
        closeFile(ctx);
        wolv::io::File saved(path, wolv::io::File::Mode::Read);
        IM_CHECK(saved.isValid());
        IM_CHECK(saved.readVector() == expected);
    };

    IMGUI_TEST_SEQUENCE("Editing", "Remove bytes at the beginning, middle and end", ctx) {
        // Leave enough bytes to check both neighbors of the middle span.
        IM_CHECK(ImHexApi::Provider::getProviders().empty());
        const TestFiles files;
        const auto path = files.create("remove-boundaries.bin", { 0x11, 0x22, 0x33, 0x44, 0x55, 0x66 });
        auto *provider = openFileByInjectedDrop(ctx, path);
        IM_CHECK(provider != nullptr && provider->isResizable());

        // Remove the first byte and verify that the remaining bytes shift down.
        ctx->SetRef("###hex.builtin.view.hex_editor.name");
        ctx->ItemClick("**/hex-byte-0", ImGuiMouseButton_Left, ImGuiTestOpFlags_NoCheckHoveredId);
        clickMenu(ctx, "hex.builtin.menu.edit"_lang, "hex.builtin.view.hex_editor.menu.edit.remove"_lang);
        ctx->SetRef("##hex.builtin.view.hex_editor.menu.edit.remove");
        ctx->ItemClick("hex.ui.common.set"_lang.get());
        IM_CHECK(hasBytes(*provider, { 0x22, 0x33, 0x44, 0x55, 0x66 }));

        // Remove two middle bytes while keeping the bytes on either side.
        ctx->SetRef("###hex.builtin.view.hex_editor.name");
        ctx->ItemClick("**/hex-byte-1", ImGuiMouseButton_Left, ImGuiTestOpFlags_NoCheckHoveredId);
        clickMenu(ctx, "hex.builtin.menu.edit"_lang, "hex.builtin.view.hex_editor.menu.edit.remove"_lang);
        ctx->SetRef("##hex.builtin.view.hex_editor.menu.edit.remove");
        ctx->ItemClick("hex.ui.common.size"_lang.get());
        ctx->KeyCharsReplace("2");
        ctx->ItemClick("hex.ui.common.set"_lang.get());
        IM_CHECK(hasBytes(*provider, { 0x22, 0x55, 0x66 }));

        // Removing the last byte should leave the prefix untouched.
        ctx->SetRef("###hex.builtin.view.hex_editor.name");
        ctx->ItemClick("**/hex-byte-2", ImGuiMouseButton_Left, ImGuiTestOpFlags_NoCheckHoveredId);
        clickMenu(ctx, "hex.builtin.menu.edit"_lang, "hex.builtin.view.hex_editor.menu.edit.remove"_lang);
        ctx->SetRef("##hex.builtin.view.hex_editor.menu.edit.remove");
        ctx->ItemClick("hex.ui.common.set"_lang.get());
        IM_CHECK(hasBytes(*provider, { 0x22, 0x55 }));

        // Verify the saved file contains exactly the surviving bytes.
        clickMenu(ctx, "hex.builtin.menu.file"_lang, "hex.builtin.view.hex_editor.menu.file.save"_lang);
        closeFile(ctx);
        IM_CHECK(ImHexApi::Provider::getProviders().empty());
        wolv::io::File saved(path, wolv::io::File::Mode::Read);
        IM_CHECK(saved.isValid());
        IM_CHECK(saved.readVector() == std::vector<u8>({ 0x22, 0x55 }));
    };

    IMGUI_TEST_SEQUENCE("Editing", "Resize an empty file and remove its only byte", ctx) {
        // Open an empty but resizable file.
        IM_CHECK(ImHexApi::Provider::getProviders().empty());
        const TestFiles files;
        const auto path = files.create("empty.bin", {});
        IM_CHECK(!path.empty());
        auto *provider = openFileByInjectedDrop(ctx, path);
        IM_CHECK(provider != nullptr && provider->isResizable());
        IM_CHECK(provider->getActualSize() == 0);

        // Insert should be unavailable without a selectable byte.
        ctx->Yield(3);
        const auto editMenu = fmt::format("//##MainMenuBar/{}", "hex.builtin.menu.edit"_lang);
        ctx->MenuAction(ImGuiTestAction_Open, editMenu.c_str());
        const auto insertItem = fmt::format("//$FOCUSED/{}", "hex.builtin.view.hex_editor.menu.edit.insert"_lang);
        const auto insertInfo = ctx->ItemInfo(insertItem.c_str());
        IM_CHECK(insertInfo.ID != 0 && (insertInfo.ItemFlags & ImGuiItemFlags_Disabled));
        ctx->KeyPress(ImGuiKey_Escape);

        // Resize can still create the first byte, initialized to zero.
        clickMenu(ctx, "hex.builtin.menu.edit"_lang, "hex.builtin.view.hex_editor.menu.edit.resize"_lang);
        ctx->SetRef("##hex.builtin.view.hex_editor.menu.edit.resize");
        ctx->ItemClick("##resize");
        ctx->KeyCharsReplace("1");
        ctx->ItemClick("hex.ui.common.set"_lang.get());
        IM_CHECK(hasBytes(*provider, { 0x00 }));

        // Save the new byte so the final save has a nonempty file to truncate.
        clickMenu(ctx, "hex.builtin.menu.file"_lang, "hex.builtin.view.hex_editor.menu.file.save"_lang);
        {
            wolv::io::File saved(path, wolv::io::File::Mode::Read);
            IM_CHECK(saved.isValid());
            IM_CHECK(saved.readVector() == std::vector<u8>({ 0x00 }));
        }

        // Remove the only byte and verify that the provider is empty again.
        ctx->SetRef("###hex.builtin.view.hex_editor.name");
        ctx->ItemClick("**/hex-byte-0", ImGuiMouseButton_Left, ImGuiTestOpFlags_NoCheckHoveredId);
        clickMenu(ctx, "hex.builtin.menu.edit"_lang, "hex.builtin.view.hex_editor.menu.edit.remove"_lang);
        ctx->SetRef("##hex.builtin.view.hex_editor.menu.edit.remove");
        ctx->ItemClick("hex.ui.common.set"_lang.get());
        IM_CHECK(provider->getActualSize() == 0);

        // Saving should truncate the file all the way back to zero bytes.
        clickMenu(ctx, "hex.builtin.menu.file"_lang, "hex.builtin.view.hex_editor.menu.file.save"_lang);
        closeFile(ctx);
        IM_CHECK(ImHexApi::Provider::getProviders().empty());
        wolv::io::File saved(path, wolv::io::File::Mode::Read);
        IM_CHECK(saved.isValid());
        IM_CHECK(saved.readVector().empty());
    };

}
