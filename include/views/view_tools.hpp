#pragma once

#include <hex.hpp>

#include "imgui.h"
#include "views/view.hpp"

#include <array>
#include <string>

namespace hex {

    namespace prv { class Provider; }

    class ViewTools : public View {
    public:
        ViewTools();
        ~ViewTools() override;

        void createView() override;
        void createMenu() override;

    private:
        char *m_mangledBuffer = nullptr;
        char *m_demangledName = nullptr;

        bool m_asciiTableShowOctal = false;

        char *m_regexInput = nullptr;
        char *m_regexPattern = nullptr;
        char *m_replacePattern = nullptr;
        std::string m_regexOutput;

        std::array<float, 4> m_pickedColor;

        void drawDemangler();
        void drawASCIITable();
        void drawRegexReplacer();
        void drawColorPicker();
    };

}