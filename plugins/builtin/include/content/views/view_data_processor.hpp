#pragma once

#include <hex.hpp>

#include <imgui.h>
#include <hex/ui/view.hpp>
#include <hex/data_processor/node.hpp>
#include <hex/data_processor/link.hpp>

#include "content/helpers/provider_extra_data.hpp"

#include <array>
#include <string>

namespace hex::plugin::builtin {

    class ViewDataProcessor : public View {
    public:
        using Workspace = ProviderExtraData::Data::DataProcessor::Workspace;

        ViewDataProcessor();
        ~ViewDataProcessor() override;

        void drawContent() override;

        static nlohmann::json saveNode(const dp::Node *node);
        static nlohmann::json saveNodes(const Workspace &workspace);

        static std::unique_ptr<dp::Node> loadNode(const nlohmann::json &data);
        static void loadNodes(Workspace &workspace, const nlohmann::json &data);

    private:
        static void eraseLink(Workspace &workspace, int id);
        static void eraseNodes(Workspace &workspace, const std::vector<int> &ids);
        static void processNodes(Workspace &workspace);

        void reloadCustomNodes();
    private:
        bool m_updateNodePositions = false;
        int m_rightClickedId = -1;
        ImVec2 m_rightClickedCoords;

        bool m_continuousEvaluation = false;

        struct CustomNode {
            std::string name;
            nlohmann::json data;
        };

        std::vector<CustomNode> m_customNodes;
    };

}