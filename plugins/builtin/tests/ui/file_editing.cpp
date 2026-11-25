#include "ui_test_helpers.hpp"

namespace hex::plugin::builtin {

    using namespace ui_test;

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

    // Exercise both ends, including inserting at the address just past EOF.
    IMGUI_TEST_SEQUENCE("Editing", "Insert at the beginning and EOF", ctx) {
        IM_CHECK(ImHexApi::Provider::getProviders().empty());
        const TestFiles files;
        const auto path = files.create("boundaries.bin", { 0x11, 0x22 });
        IM_CHECK(!path.empty());
        auto *provider = openFileByInjectedDrop(ctx, path);
        IM_CHECK(provider != nullptr && provider->isResizable());

        ctx->SetRef("###hex.builtin.view.hex_editor.name");
        ctx->ItemClick("**/hex-byte-0", ImGuiMouseButton_Left, ImGuiTestOpFlags_NoCheckHoveredId);
        clickMenu(ctx, "hex.builtin.menu.edit"_lang, "hex.builtin.view.hex_editor.menu.edit.insert"_lang);
        ctx->SetRef("##hex.builtin.view.hex_editor.menu.edit.insert");
        ctx->ItemClick("hex.ui.common.size"_lang.get());
        ctx->KeyCharsReplace("1");
        ctx->ItemClick("hex.ui.common.set"_lang.get());
        IM_CHECK(hasBytes(*provider, { 0x00, 0x11, 0x22 }));

        ctx->SetRef("###hex.builtin.view.hex_editor.name");
        ctx->ItemClick("**/hex-byte-2", ImGuiMouseButton_Left, ImGuiTestOpFlags_NoCheckHoveredId);
        clickMenu(ctx, "hex.builtin.menu.edit"_lang, "hex.builtin.view.hex_editor.menu.edit.insert"_lang);
        ctx->SetRef("##hex.builtin.view.hex_editor.menu.edit.insert");
        ctx->ItemClick("hex.ui.common.address"_lang.get());
        ctx->KeyCharsReplace("3");
        ctx->ItemClick("hex.ui.common.size"_lang.get());
        ctx->KeyCharsReplace("1");
        ctx->ItemClick("hex.ui.common.set"_lang.get());
        IM_CHECK(hasBytes(*provider, { 0x00, 0x11, 0x22, 0x00 }));

        clickMenu(ctx, "hex.builtin.menu.file"_lang, "hex.builtin.view.hex_editor.menu.file.save"_lang);
        closeFile(ctx);
        wolv::io::File saved(path, wolv::io::File::Mode::Read);
        IM_CHECK(saved.isValid());
        IM_CHECK(saved.readVector() == std::vector<u8>({ 0x00, 0x11, 0x22, 0x00 }));
    };

    // Remove the ends and a middle span without losing the bytes around it.
    IMGUI_TEST_SEQUENCE("Editing", "Remove bytes at the beginning, middle and end", ctx) {
        IM_CHECK(ImHexApi::Provider::getProviders().empty());
        const TestFiles files;
        const auto path = files.create("remove-boundaries.bin", { 0x11, 0x22, 0x33, 0x44, 0x55, 0x66 });
        auto *provider = openFileByInjectedDrop(ctx, path);
        IM_CHECK(provider != nullptr && provider->isResizable());

        ctx->SetRef("###hex.builtin.view.hex_editor.name");
        ctx->ItemClick("**/hex-byte-0", ImGuiMouseButton_Left, ImGuiTestOpFlags_NoCheckHoveredId);
        clickMenu(ctx, "hex.builtin.menu.edit"_lang, "hex.builtin.view.hex_editor.menu.edit.remove"_lang);
        ctx->SetRef("##hex.builtin.view.hex_editor.menu.edit.remove");
        ctx->ItemClick("hex.ui.common.set"_lang.get());
        IM_CHECK(hasBytes(*provider, { 0x22, 0x33, 0x44, 0x55, 0x66 }));

        ctx->SetRef("###hex.builtin.view.hex_editor.name");
        ctx->ItemClick("**/hex-byte-1", ImGuiMouseButton_Left, ImGuiTestOpFlags_NoCheckHoveredId);
        clickMenu(ctx, "hex.builtin.menu.edit"_lang, "hex.builtin.view.hex_editor.menu.edit.remove"_lang);
        ctx->SetRef("##hex.builtin.view.hex_editor.menu.edit.remove");
        ctx->ItemClick("hex.ui.common.size"_lang.get());
        ctx->KeyCharsReplace("2");
        ctx->ItemClick("hex.ui.common.set"_lang.get());
        IM_CHECK(hasBytes(*provider, { 0x22, 0x55, 0x66 }));

        ctx->SetRef("###hex.builtin.view.hex_editor.name");
        ctx->ItemClick("**/hex-byte-2", ImGuiMouseButton_Left, ImGuiTestOpFlags_NoCheckHoveredId);
        clickMenu(ctx, "hex.builtin.menu.edit"_lang, "hex.builtin.view.hex_editor.menu.edit.remove"_lang);
        ctx->SetRef("##hex.builtin.view.hex_editor.menu.edit.remove");
        ctx->ItemClick("hex.ui.common.set"_lang.get());
        IM_CHECK(hasBytes(*provider, { 0x22, 0x55 }));

        clickMenu(ctx, "hex.builtin.menu.file"_lang, "hex.builtin.view.hex_editor.menu.file.save"_lang);
        closeFile(ctx);
        IM_CHECK(ImHexApi::Provider::getProviders().empty());
        wolv::io::File saved(path, wolv::io::File::Mode::Read);
        IM_CHECK(saved.isValid());
        IM_CHECK(saved.readVector() == std::vector<u8>({ 0x22, 0x55 }));
    };

    // An empty file has no selectable byte; resize is still available.
    IMGUI_TEST_SEQUENCE("Editing", "Resize an empty file and remove its only byte", ctx) {
        IM_CHECK(ImHexApi::Provider::getProviders().empty());
        const TestFiles files;
        const auto path = files.create("empty.bin", {});
        IM_CHECK(!path.empty());
        auto *provider = openFileByInjectedDrop(ctx, path);
        IM_CHECK(provider != nullptr && provider->isResizable());
        IM_CHECK(provider->getActualSize() == 0);

        ctx->Yield(3);
        const auto editMenu = fmt::format("//##MainMenuBar/{}", "hex.builtin.menu.edit"_lang);
        ctx->MenuAction(ImGuiTestAction_Open, editMenu.c_str());
        const auto insertItem = fmt::format("//$FOCUSED/{}", "hex.builtin.view.hex_editor.menu.edit.insert"_lang);
        const auto insertInfo = ctx->ItemInfo(insertItem.c_str());
        IM_CHECK(insertInfo.ID != 0 && (insertInfo.ItemFlags & ImGuiItemFlags_Disabled));
        ctx->KeyPress(ImGuiKey_Escape);

        clickMenu(ctx, "hex.builtin.menu.edit"_lang, "hex.builtin.view.hex_editor.menu.edit.resize"_lang);
        ctx->SetRef("##hex.builtin.view.hex_editor.menu.edit.resize");
        ctx->ItemClick("##resize");
        ctx->KeyCharsReplace("1");
        ctx->ItemClick("hex.ui.common.set"_lang.get());
        IM_CHECK(hasBytes(*provider, { 0x00 }));

        ctx->SetRef("###hex.builtin.view.hex_editor.name");
        ctx->ItemClick("**/hex-byte-0", ImGuiMouseButton_Left, ImGuiTestOpFlags_NoCheckHoveredId);
        clickMenu(ctx, "hex.builtin.menu.edit"_lang, "hex.builtin.view.hex_editor.menu.edit.remove"_lang);
        ctx->SetRef("##hex.builtin.view.hex_editor.menu.edit.remove");
        ctx->ItemClick("hex.ui.common.set"_lang.get());
        IM_CHECK(provider->getActualSize() == 0);
        clickMenu(ctx, "hex.builtin.menu.file"_lang, "hex.builtin.view.hex_editor.menu.file.save"_lang);
        closeFile(ctx);
        IM_CHECK(ImHexApi::Provider::getProviders().empty());
        wolv::io::File saved(path, wolv::io::File::Mode::Read);
        IM_CHECK(saved.isValid());
        IM_CHECK(saved.readVector().empty());
    };

}
