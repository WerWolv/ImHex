#pragma once

#include <imgui.h>
#include <hex/ui/view.hpp>
#include <hex/data_processor/node.hpp>
#include <hex/data_processor/link.hpp>

#include <imnodes_internal.h>

#include <string>
#include <hex/api/task_manager.hpp>
#include <nlohmann/json.hpp>

namespace hex::plugin::builtin {

    class ViewDataProcessor : public View::Window {
    public:
        struct Workspace {
            Workspace() = default;

            std::unique_ptr<ImNodesContext, void(*)(ImNodesContext*)> context = { []{
                ImNodesContext *ctx = ImNodes::CreateContext();
                ctx->Style = ImNodes::GetStyle();
                ctx->Io = ImNodes::GetIO();
                ctx->AttributeFlagStack = GImNodes->AttributeFlagStack;

                return ctx;
            }(), [](ImNodesContext *ptr) {
                if (ptr != nullptr)
                    ImNodes::DestroyContext(ptr);
            } };

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

        static std::unique_ptr<dp::Node> loadNode(nlohmann::json data);
        void loadNodes(Workspace &workspace, nlohmann::json data);

        static void eraseLink(Workspace &workspace, int id);
        static void eraseNodes(Workspace &workspace, const std::vector<int> &ids);
        void processNodes(Workspace &workspace);

        void reloadCustomNodes();
        void updateNodePositions();

        std::vector<Workspace*> &getWorkspaceStack() { return *m_workspaceStack; }

    private:
        void drawContextMenus(ViewDataProcessor::Workspace &workspace);
        void drawNode(dp::Node &node) const;

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
        TaskHolder m_evaluationTask;
    };

}