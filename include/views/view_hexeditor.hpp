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

#include "providers/provider.hpp"

namespace hex {

    class ViewHexEditor : public View {
    public:
        ViewHexEditor(prv::Provider* &dataProvider, std::vector<Highlight> &highlights);
        virtual ~ViewHexEditor();

        virtual void createView() override;
        virtual void createMenu() override;

    private:
        MemoryEditor m_memoryEditor;

        prv::Provider* &m_dataProvider;

        std::vector<Highlight> &m_highlights;
    };

}