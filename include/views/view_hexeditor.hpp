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

    class ViewHexEditor : public View {
    public:
        ViewHexEditor(FILE* &file, std::vector<Highlight> &highlights);
        virtual ~ViewHexEditor();

        virtual void createView() override;
        virtual void createMenu() override;

    private:
        MemoryEditor m_memoryEditor;

        FILE* &m_file;
        size_t m_fileSize = 0;

        std::vector<Highlight> &m_highlights;
    };

}