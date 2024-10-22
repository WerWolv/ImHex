#include <hex/api/imhex_api.hpp>
#include <hex/api/content_registry.hpp>
#include <hex/providers/provider.hpp>
#include <hex/helpers/magic.hpp>

#include <pl/core/evaluator.hpp>

namespace hex::plugin::builtin {

    void registerPatternLanguagePragmas() {

        ContentRegistry::PatternLanguage::addPragma("base_address", [](pl::PatternLanguage &runtime, const std::string &value) {
            auto baseAddress = strtoull(value.c_str(), nullptr, 0);

            ImHexApi::Provider::get()->setBaseAddress(baseAddress);
            runtime.setDataBaseAddress(baseAddress);

            return true;
        });

        ContentRegistry::PatternLanguage::addPragma("MIME", [](pl::PatternLanguage&, const std::string &value) {
            return magic::isValidMIMEType(value);
        });

        ContentRegistry::PatternLanguage::addPragma("magic", [](pl::PatternLanguage&, const std::string &) {
            return true;
        });
    }

}
