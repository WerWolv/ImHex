#pragma once

#include <windows.h>
#include <shobjidl.h>

#include "utils.hpp"
#include "views/view.hpp"

#include "imgui_memory_editor.h"

#include <tuple>
#include <random>
#include <vector>

#include "views/highlight.hpp"

namespace hex {

    namespace prv { class Provider; }

    class ViewHexEditor : public View {
    public:
        ViewHexEditor(prv::Provider* &dataProvider, std::vector<Highlight> &highlights);
        ~ViewHexEditor() override;

        void createView() override;
        void createMenu() override;

    private:
        MemoryEditor m_memoryEditor;

        prv::Provider* &m_dataProvider;

        std::vector<Highlight> &m_highlights;
    };

}