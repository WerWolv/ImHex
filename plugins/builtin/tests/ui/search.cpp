#include "ui_test_helpers.hpp"

#include <hex/api/content_registry/views.hpp>
#include <hex/api/imhex_api/hex_editor.hpp>
#include <hex/api/imhex_api/system.hpp>
#include <hex/ui/view.hpp>

#include <fonts/vscode_icons.hpp>

namespace hex::plugin::builtin {

    using namespace ui_test;

    namespace {

        const std::vector<u8> SearchBytes = {
            'a', 'l', 'p', 'h', 'a', 0, 'A', 'L', 'P', 'H', 'A', 0, 0x42, 0x43, 0,
            0xDE, 0xAD, 0xBE, 0xEF, 0x13, 0x37, 0xCE, 0xFA, 0
        };

        // Use a private constants directory rather than relying on installed lists.
        class TestConstants {
        public:
            explicit TestConstants(const std::fs::path &directory)
                : m_previousPaths(ImHexApi::System::getAdditionalFolderPaths()) {
                if (directory.empty())
                    return;

                std::error_code error;
                const auto constants = directory / "constants";
                if (!std::fs::create_directory(constants, error))
                    return;

                wolv::io::File file(constants / "test.json", wolv::io::File::Mode::Create);
                const std::string content = R"({"name":"Test","values":[{"name":"Marker","value":"DE AD BE EF 13 37 CE FA"}]})";
                if (!file.isValid() || file.writeString(content) != static_cast<i64>(content.size()))
                    return;
                file.close();

                auto paths = m_previousPaths;
                paths.push_back(directory);
                ImHexApi::System::setAdditionalFolderPaths(paths);
                m_ready = true;
            }

            ~TestConstants() {
                if (m_ready)
                    ImHexApi::System::setAdditionalFolderPaths(m_previousPaths);
            }

            bool ready() const {
                return m_ready;
            }

        private:
            std::vector<std::fs::path> m_previousPaths;
            bool m_ready = false;
        };

        // Wait for the search and its deferred result-table update.
        bool waitForSearch(ImGuiTestContext *ctx) {
            ctx->Yield();
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
            while (!ctx->IsError() && TaskManager::getRunningTaskCount() != 0 &&
                   std::chrono::steady_clock::now() < deadline)
                ctx->Yield();
            ctx->Yield(3);
            return TaskManager::getRunningTaskCount() == 0;
        }

        void openFind(ImGuiTestContext *ctx) {
            auto *view = ContentRegistry::Views::getViewByName("hex.builtin.view.find.name"_unlocalized);
            view->getWindowOpenState() = true;
            ctx->Yield(3);
            ctx->WindowFocus("###hex.builtin.view.find.name");
            ctx->Yield();
            ctx->SetRef("###hex.builtin.view.find.name");
        }

        // Search settings are in a child window; wildcard lookup follows its parent.
        std::string item(const char *label) {
            return fmt::format("**/{}", label);
        }

        void tab(ImGuiTestContext *ctx, const char *label) {
            ctx->ItemClick(item(label).c_str());
            ctx->Yield();
        }

        void input(ImGuiTestContext *ctx, const char *label, const char *value) {
            ctx->ItemClick(item(label).c_str());
            ctx->KeyCharsReplace(value);
        }

        // The input and its enclosing tab have the same label.
        void binaryPatternInput(ImGuiTestContext *ctx, const char *value) {
            const auto label = "hex.builtin.view.find.binary_pattern"_lang;
            ctx->ItemClick(fmt::format("**/{}/{}", label, label).c_str());
            ctx->KeyCharsReplace(value);
        }

        bool search(ImGuiTestContext *ctx) {
            ctx->ItemClick(ICON_VS_SEARCH);
            return waitForSearch(ctx);
        }

        void expandSettings(ImGuiTestContext *ctx) {
            ctx->ItemOpen("**/##CollapseHeader");
        }

        // PushID(int) uses a binary integer hash, represented by $$ in test paths.
        std::string row(size_t index) {
            return fmt::format("**/$${}/##line", index);
        }

        void selectRow(ImGuiTestContext *ctx, size_t index) {
            ctx->ItemClick(row(index).c_str());
        }

        bool searchEnabled(ImGuiTestContext *ctx) {
            return (ctx->ItemInfo(ICON_VS_SEARCH).ItemFlags & ImGuiItemFlags_Disabled) == 0;
        }

        bool selectionIs(u64 address, u64 size) {
            const auto selection = ImHexApi::HexEditor::getSelection();
            return selection.has_value() && selection->getStartAddress() == address && selection->getSize() == size;
        }

    }

    IMGUI_TEST_SEQUENCE("Find", "Extract strings with a minimum length", ctx) {
        IM_CHECK(ImHexApi::Provider::getProviders().empty());
        const TestFiles files;
        const auto path = files.create("search.bin", SearchBytes);
        IM_CHECK(openFileByInjectedDrop(ctx, path));
        openFind(ctx);
        tab(ctx, "hex.builtin.view.find.strings"_lang);
        ctx->ItemClick(item("hex.ui.common.range.entire_data"_lang).c_str());
        input(ctx, "hex.builtin.view.find.strings.min_length"_lang, "5");
        IM_CHECK(search(ctx));
        selectRow(ctx, 0);
        IM_CHECK(selectionIs(0, 5));
        selectRow(ctx, 1);
        IM_CHECK(selectionIs(6, 5));
        IM_CHECK(!ctx->ItemExists(row(2).c_str()));
    };

    IMGUI_TEST_SEQUENCE("Find", "Sequence search and result navigation", ctx) {
        IM_CHECK(ImHexApi::Provider::getProviders().empty());
        const TestFiles files;
        const auto path = files.create("search.bin", SearchBytes);
        IM_CHECK(openFileByInjectedDrop(ctx, path));
        openFind(ctx);
        tab(ctx, "hex.builtin.view.find.sequences"_lang);
        ctx->ItemClick(item("hex.ui.common.range.entire_data"_lang).c_str());
        input(ctx, "hex.ui.common.value"_lang, "alpha");
        ctx->ItemCheck(item("hex.builtin.view.find.sequences.ignore_case"_lang).c_str());
        IM_CHECK(search(ctx));
        selectRow(ctx, 0);
        IM_CHECK(selectionIs(0, 5));
        selectRow(ctx, 1);
        IM_CHECK(selectionIs(6, 5));
        IM_CHECK(!ctx->ItemExists(row(2).c_str()));
        ctx->ItemClick(ICON_VS_SEARCH_STOP);
        IM_CHECK(!ctx->ItemExists(row(0).c_str()));

        // Searching only the current selection must not return the first match.
        expandSettings(ctx);
        ctx->ItemClick(item("hex.ui.common.range.selection"_lang).c_str());
        IM_CHECK(search(ctx));
        selectRow(ctx, 0);
        IM_CHECK(selectionIs(6, 5));
        IM_CHECK(!ctx->ItemExists(row(1).c_str()));
    };

    IMGUI_TEST_SEQUENCE("Find", "Regex validation and matching", ctx) {
        IM_CHECK(ImHexApi::Provider::getProviders().empty());
        const TestFiles files;
        const auto path = files.create("search.bin", SearchBytes);
        IM_CHECK(openFileByInjectedDrop(ctx, path));
        openFind(ctx);
        tab(ctx, "hex.builtin.view.find.regex"_lang);
        ctx->ItemClick(item("hex.ui.common.range.entire_data"_lang).c_str());
        input(ctx, "hex.builtin.view.find.regex.pattern"_lang, "[");
        ctx->Yield();
        IM_CHECK(!searchEnabled(ctx));
        input(ctx, "hex.builtin.view.find.regex.pattern"_lang, "a.*a");
        ctx->ItemCheck(item("hex.builtin.view.find.regex.full_match"_lang).c_str());
        IM_CHECK(search(ctx));
        selectRow(ctx, 0);
        IM_CHECK(selectionIs(0, 5));
        IM_CHECK(!ctx->ItemExists(row(1).c_str()));
    };

    IMGUI_TEST_SEQUENCE("Find", "Binary wildcard and no matches", ctx) {
        IM_CHECK(ImHexApi::Provider::getProviders().empty());
        const TestFiles files;
        const auto path = files.create("search.bin", SearchBytes);
        IM_CHECK(openFileByInjectedDrop(ctx, path));
        openFind(ctx);
        tab(ctx, "hex.builtin.view.find.binary_pattern"_lang);
        ctx->ItemClick(item("hex.ui.common.range.entire_data"_lang).c_str());
        binaryPatternInput(ctx, "61 6C ?? 68 61");
        IM_CHECK(search(ctx));
        selectRow(ctx, 0);
        IM_CHECK(selectionIs(0, 5));
        IM_CHECK(!ctx->ItemExists(row(1).c_str()));

        expandSettings(ctx);
        binaryPatternInput(ctx, "FF FF FF");
        IM_CHECK(search(ctx));
        IM_CHECK(!ctx->ItemExists(row(0).c_str()));
    };

    IMGUI_TEST_SEQUENCE("Find", "Numeric value search", ctx) {
        IM_CHECK(ImHexApi::Provider::getProviders().empty());
        const TestFiles files;
        const auto path = files.create("search.bin", SearchBytes);
        IM_CHECK(openFileByInjectedDrop(ctx, path));
        openFind(ctx);
        tab(ctx, "hex.builtin.view.find.value"_lang);
        ctx->ItemClick(item("hex.ui.common.range.entire_data"_lang).c_str());
        ctx->ItemUncheck(item("hex.builtin.view.find.value.range"_lang).c_str());
        input(ctx, "hex.ui.common.value"_lang, "66");
        IM_CHECK(search(ctx));
        selectRow(ctx, 0);
        IM_CHECK(selectionIs(12, 1));
        IM_CHECK(!ctx->ItemExists(row(1).c_str()));
    };

    IMGUI_TEST_SEQUENCE("Find", "Find a constant from a private list", ctx) {
        IM_CHECK(ImHexApi::Provider::getProviders().empty());
        const TestFiles files;
        const auto path = files.create("search.bin", SearchBytes);
        const TestConstants constants(files.directory());
        IM_CHECK(constants.ready());
        IM_CHECK(openFileByInjectedDrop(ctx, path));
        openFind(ctx);

        // Constants inherits validity from the previous tab in the current UI.
        tab(ctx, "hex.builtin.view.find.strings"_lang);
        ctx->ItemClick(item("hex.ui.common.range.entire_data"_lang).c_str());
        tab(ctx, "hex.builtin.view.find.constants"_lang);
        IM_CHECK(search(ctx));

        // Other installed constant lists may also match, so locate our marker by region.
        bool found = false;
        for (size_t index = 0; index < 100 && ctx->ItemExists(row(index).c_str()); index++) {
            selectRow(ctx, index);
            if (selectionIs(15, 8)) {
                found = true;
                break;
            }
        }
        IM_CHECK(found);
    };

}
