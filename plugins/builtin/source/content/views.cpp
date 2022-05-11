//#include "content/views/view_hex_editor.hpp"
#include "content/views/view_hex_editor_new.hpp"
#include "content/views/view_pattern_editor.hpp"
#include "content/views/view_pattern_data.hpp"
#include "content/views/view_hashes.hpp"
#include "content/views/view_information.hpp"
#include "content/views/view_about.hpp"
#include "content/views/view_tools.hpp"
#include "content/views/view_strings.hpp"
#include "content/views/view_data_inspector.hpp"
#include "content/views/view_disassembler.hpp"
#include "content/views/view_bookmarks.hpp"
#include "content/views/view_patches.hpp"
#include "content/views/view_command_palette.hpp"
#include "content/views/view_settings.hpp"
#include "content/views/view_data_processor.hpp"
#include "content/views/view_yara.hpp"
#include "content/views/view_constants.hpp"
#include "content/views/view_store.hpp"
#include "content/views/view_diff.hpp"
#include "content/views/view_provider_settings.hpp"

namespace hex::plugin::builtin {

    void registerViews() {
        //ContentRegistry::Views::add<ViewHexEditor>();
        ContentRegistry::Views::add<ViewHexEditorNew>();
        ContentRegistry::Views::add<ViewPatternEditor>();
        ContentRegistry::Views::add<ViewPatternData>();
        ContentRegistry::Views::add<ViewDataInspector>();
        ContentRegistry::Views::add<ViewHashes>();
        ContentRegistry::Views::add<ViewInformation>();
        ContentRegistry::Views::add<ViewStrings>();
        ContentRegistry::Views::add<ViewDisassembler>();
        ContentRegistry::Views::add<ViewBookmarks>();
        ContentRegistry::Views::add<ViewPatches>();
        ContentRegistry::Views::add<ViewTools>();
        ContentRegistry::Views::add<ViewCommandPalette>();
        ContentRegistry::Views::add<ViewAbout>();
        ContentRegistry::Views::add<ViewSettings>();
        ContentRegistry::Views::add<ViewDataProcessor>();
        ContentRegistry::Views::add<ViewYara>();
        ContentRegistry::Views::add<ViewConstants>();
        ContentRegistry::Views::add<ViewStore>();
        ContentRegistry::Views::add<ViewDiff>();
        ContentRegistry::Views::add<ViewProviderSettings>();
    }

}