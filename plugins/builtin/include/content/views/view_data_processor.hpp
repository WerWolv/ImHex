#pragma once

#include <hex.hpp>

#include <imgui.h>
#include <hex/ui/view.hpp>
#include <hex/data_processor/node.hpp>
#include <hex/data_processor/link.hpp>

#include <array>
#include <string>

namespace hex::plugin::builtin {

    class ViewDataProcessor : public View {
    public:
        ViewDataProcessor();
        ~ViewDataProcessor() override;

        void drawContent() override;

    private:
        int m_rightClickedId = -1;
        ImVec2 m_rightClickedCoords;

        bool m_continuousEvaluation = false;

        void eraseLink(u32 id);
        void eraseNodes(const std::vector<int> &ids);
        void processNodes();

        std::string saveNodes(prv::Provider *provider);
        void loadNodes(prv::Provider *provider, const std::string &data);
    };

}