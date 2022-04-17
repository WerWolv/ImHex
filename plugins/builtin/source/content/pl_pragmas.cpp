#include <hex/api/content_registry.hpp>
#include <hex/providers/provider.hpp>

#include <pl/evaluator.hpp>

namespace hex::plugin::builtin {

    void registerPatternLanguagePragmas() {
        ContentRegistry::PatternLanguage::addPragma("endian", [](pl::PatternLanguage &runtime, const std::string &value) {
            if (value == "big") {
                runtime.getInternals().evaluator->setDefaultEndian(std::endian::big);
                return true;
            } else if (value == "little") {
                runtime.getInternals().evaluator->setDefaultEndian(std::endian::little);
                return true;
            } else if (value == "native") {
                runtime.getInternals().evaluator->setDefaultEndian(std::endian::native);
                return true;
            } else
                return false;
        });

        ContentRegistry::PatternLanguage::addPragma("eval_depth", [](pl::PatternLanguage &runtime, const std::string &value) {
            auto limit = strtol(value.c_str(), nullptr, 0);

            if (limit <= 0)
                return false;

            runtime.getInternals().evaluator->setEvaluationDepth(limit);
            return true;
        });

        ContentRegistry::PatternLanguage::addPragma("array_limit", [](pl::PatternLanguage &runtime, const std::string &value) {
            auto limit = strtol(value.c_str(), nullptr, 0);

            if (limit <= 0)
                return false;

            runtime.getInternals().evaluator->setArrayLimit(limit);
            return true;
        });

        ContentRegistry::PatternLanguage::addPragma("pattern_limit", [](pl::PatternLanguage &runtime, const std::string &value) {
            auto limit = strtol(value.c_str(), nullptr, 0);

            if (limit <= 0)
                return false;

            runtime.getInternals().evaluator->setPatternLimit(limit);
            return true;
        });

        ContentRegistry::PatternLanguage::addPragma("loop_limit", [](pl::PatternLanguage &runtime, const std::string &value) {
            auto limit = strtol(value.c_str(), nullptr, 0);

            if (limit <= 0)
                return false;

            runtime.getInternals().evaluator->setLoopLimit(limit);
            return true;
        });

        ContentRegistry::PatternLanguage::addPragma("base_address", [](pl::PatternLanguage &runtime, const std::string &value) {
            hex::unused(runtime);

            auto baseAddress = strtoull(value.c_str(), nullptr, 0);

            ImHexApi::Provider::get()->setBaseAddress(baseAddress);
            return true;
        });

        ContentRegistry::PatternLanguage::addPragma("bitfield_order", [](pl::PatternLanguage &runtime, const std::string &value) {
            if (value == "left_to_right") {
                runtime.getInternals().evaluator->setBitfieldOrder(pl::BitfieldOrder::LeftToRight);
                return true;
            } else if (value == "right_to_left") {
                runtime.getInternals().evaluator->setBitfieldOrder(pl::BitfieldOrder::RightToLeft);
                return true;
            } else {
                return false;
            }
        });
    }

}