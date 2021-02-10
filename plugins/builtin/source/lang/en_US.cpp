#include <hex/plugin.hpp>

namespace hex::plugin::builtin {

    void registerLanguageEnUS() {
        ContentRegistry::Language::registerLanguage("English (US)", "en-US");

        ContentRegistry::Language::addLocalizations("en-US", {

        });
    }

}