#pragma once

#include <hex.hpp>

#include <imgui.h>
#include <hex/views/view.hpp>
#include <hex/data_processor/node.hpp>
#include <hex/data_processor/link.hpp>

#include <array>
#include <string>

namespace hex {

    namespace prv { class Provider; }

    class ViewDataProcessor : public View {
    public:
        ViewDataProcessor();
        ~ViewDataProcessor() override;

        void drawContent() override;
        void drawMenu() override;

    private:
        std::list<dp::Node*> m_endNodes;
        std::list<dp::Node*> m_nodes;
        std::list<dp::Link>  m_links;

        std::vector<prv::Overlay*> m_dataOverlays;

        int m_rightClickedId = -1;
        ImVec2 m_rightClickedCoords;

        std::optional<dp::Node::NodeError> m_currNodeError;

        void eraseLink(u32 id);
        void eraseNodes(const std::vector<int> &ids);
        void processNodes();

        std::string saveNodes();
        void loadNodes(std::string_view data);
    };

}