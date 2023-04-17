#pragma once

#include <hex.hpp>

#include <imgui.h>
#include <hex/ui/view.hpp>
#include <hex/data_processor/node.hpp>
#include <hex/data_processor/link.hpp>

#include <imnodes_internal.h>

#include <array>
#include <string>

namespace hex::plugin::builtin {

    class ViewDataProcessor : public View {
    public:
        struct Workspace {
            Workspace() = default;

            std::unique_ptr<ImNodesContext, void(*)(ImNodesContext*)> context = { []{
                ImNodesContext *ctx = ImNodes::CreateContext();
                ctx->Style = ImNodes::GetStyle();
                ctx->Io = ImNodes::GetIO();
                ctx->AttributeFlagStack = GImNodes->AttributeFlagStack;

                return ctx;
            }(), ImNodes::DestroyContext };

            std::list<std::unique_ptr<dp::Node>> nodes;
            std::list<dp::Node*> endNodes;
            std::list<dp::Link> links;
            std::vector<hex::prv::Overlay *> dataOverlays;
            std::optional<dp::Node::NodeError> currNodeError;
        };

    public:
        ViewDataProcessor();
        ~ViewDataProcessor() override;

        void drawContent() override;

        static nlohmann::json saveNode(const dp::Node *node);
        static nlohmann::json saveNodes(const Workspace &workspace);

        static std::unique_ptr<dp::Node> loadNode(const nlohmann::json &data);
        static void loadNodes(Workspace &workspace, const nlohmann::json &data);

        static void eraseLink(Workspace &workspace, int id);
        static void eraseNodes(Workspace &workspace, const std::vector<int> &ids);
        static void processNodes(Workspace &workspace);

        void reloadCustomNodes();

        std::vector<Workspace*> &getWorkspaceStack() { return *this->m_workspaceStack; }

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

        PerProvider<Workspace> m_mainWorkspace;
        PerProvider<std::vector<Workspace*>> m_workspaceStack;
    };

}