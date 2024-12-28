#include <hex/plugin.hpp>

#include <hex/api/content_registry.hpp>
#include <hex/helpers/logger.hpp>

#include <pl/api.hpp>

#include <romfs/romfs.hpp>

#include "content/views/view_disassembler.hpp"

using namespace hex;
using namespace hex::plugin::disasm;

namespace hex::plugin::disasm {

    void drawDisassemblyVisualizer(pl::ptrn::Pattern &, bool, std::span<const pl::core::Token::Literal> arguments);
    void registerPatternLanguageTypes();

    void registerCapstoneArchitectures();
    void registerCustomArchitectures();

}

namespace {

    void registerViews() {
        ContentRegistry::Views::add<ViewDisassembler>();
    }

    void registerPlVisualizers() {
        using ParamCount = pl::api::FunctionParameterCount;

        ContentRegistry::PatternLanguage::addVisualizer("disassembler", drawDisassemblyVisualizer, ParamCount::exactly(3));
    }

}


IMHEX_PLUGIN_SETUP("Disassembler", "WerWolv", "Disassembler support") {
    hex::log::debug("Using romfs: '{}'", romfs::name());
    for (auto &path : romfs::list("lang"))
        hex::ContentRegistry::Language::addLocalization(nlohmann::json::parse(romfs::get(path).string()));

    registerViews();
    registerPlVisualizers();
    registerPatternLanguageTypes();

    registerCapstoneArchitectures();
    registerCustomArchitectures();
}
