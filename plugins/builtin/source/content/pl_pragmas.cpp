#include <hex/api/content_registry.hpp>
#include <hex/providers/provider.hpp>

#include <pl/core/evaluator.hpp>

namespace hex::plugin::builtin {

    void registerPatternLanguagePragmas() {

        ContentRegistry::PatternLanguage::addPragma("base_address", [](pl::PatternLanguage &runtime, const std::string &value) {
            hex::unused(runtime);

            auto baseAddress = strtoull(value.c_str(), nullptr, 0);

            ImHexApi::Provider::get()->setBaseAddress(baseAddress);
            return true;
        });

        ContentRegistry::PatternLanguage::addPragma("MIME", [](pl::PatternLanguage&, const std::string &value) { return !value.empty(); });
    }

}