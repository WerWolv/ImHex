#pragma once

#include <string>
#include <map>
#include "hex/api/content_registry.hpp"

namespace  hex::ui {

    class VisualizerDrawer {
        std::string m_lastVisualizerError;
    public:
        VisualizerDrawer()=default;
        void drawVisualizer(const std::map<std::string, ContentRegistry::PatternLanguage::impl::Visualizer> &visualizers, const std::vector<pl::core::Token::Literal> &arguments, pl::ptrn::Pattern &pattern, bool reset);
        const std::string& getLastVisualizerError() const {
           return m_lastVisualizerError;
        }
        void setLastVisualizerError(const std::string &error) {
            m_lastVisualizerError = error;
        }
        void clearLastVisualizerError() {
            m_lastVisualizerError.clear();
        }
    };
}