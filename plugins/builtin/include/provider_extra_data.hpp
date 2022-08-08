#pragma once

#include <map>
#include <hex/providers/provider.hpp>

#include <pl/pattern_language.hpp>
#include <hex/data_processor/attribute.hpp>
#include <hex/data_processor/node.hpp>
#include <hex/data_processor/link.hpp>

namespace hex::plugin::builtin {

    class ProviderExtraData {
    public:
        struct Data {
            bool dataDirty = false;

            struct {
                std::string sourceCode;
                std::unique_ptr<pl::PatternLanguage> runtime;
            } patternLanguage;

            std::list<ImHexApi::Bookmarks::Entry> bookmarks;

            struct {
                std::list<dp::Node*> endNodes;
                std::list<std::unique_ptr<dp::Node>> nodes;
                std::list<dp::Link> links;

                std::vector<hex::prv::Overlay *> dataOverlays;
                std::optional<dp::Node::NodeError> currNodeError;
            } dataProcessor;
        };

        static Data& getCurrent() {
            return get(ImHexApi::Provider::get());
        }

        static Data& get(hex::prv::Provider *provider) {
            return s_data[provider];
        }

        static void erase(hex::prv::Provider *provider) {
            s_data.erase(provider);
        }

        static bool markDirty() {
            return getCurrent().dataDirty = true;
        }

    private:
        ProviderExtraData() = default;
        static inline std::map<hex::prv::Provider*, Data> s_data = {};
    };

}