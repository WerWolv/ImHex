#include "views/view_data_processor.hpp"

#include <hex/providers/provider.hpp>

#include <imnodes.h>

namespace hex {

    ViewDataProcessor::ViewDataProcessor() : View("Data Processor") {
        imnodes::Initialize();
    }

    ViewDataProcessor::~ViewDataProcessor() {
        imnodes::Shutdown();
    }


    auto ViewDataProcessor::eraseLink(u32 id) {
        auto link = std::find_if(this->m_links.begin(), this->m_links.end(), [&id](auto link){ return link.getID() == id; });
        for (auto &node : this->m_nodes) {
            for (auto &attribute : node->getAttributes()) {
                if (attribute.getID() == link->getFromID() || attribute.getID() == link->getToID())
                    attribute.setConnectedAttribute(0, nullptr);
            }
        }

        this->m_links.erase(link);
    }

    void ViewDataProcessor::processNodes() {
        for (auto &endNode : this->m_endNodes)
            endNode->process();
    }

    void ViewDataProcessor::drawContent() {
        if (ImGui::Begin("Data Processor", &this->getWindowOpenState(), ImGuiWindowFlags_NoCollapse)) {
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) ImGui::OpenPopup("Add Node");

            if (ImGui::BeginPopup("Add Node")) {
                Node *node = nullptr;
                if (ImGui::MenuItem("Integer Node")) {
                    node = new NodeInteger();
                }
                if (ImGui::MenuItem("Read Data Node")) {
                    node = new NodeReadData();
                }
                if (ImGui::MenuItem("Invert Data Node")) {
                    node = new NodeInvert();
                }
                if (ImGui::MenuItem("Write Data Node")) {
                    node = new NodeWriteData();
                }

                if (node != nullptr) {
                    this->m_nodes.push_back(node);

                    if (node->isEndNode())
                        this->m_endNodes.push_back(node);
                }

                ImGui::EndPopup();
            }

            imnodes::BeginNodeEditor();

            for (auto& node : this->m_nodes) {
                imnodes::BeginNode(node->getID());

                imnodes::BeginNodeTitleBar();
                ImGui::TextUnformatted(node->getTitle().data());
                imnodes::EndNodeTitleBar();

                node->drawNode();

                for (auto& attribute : node->getAttributes()) {
                    if (attribute.getType() == Attribute::Type::In) {
                        imnodes::BeginInputAttribute(attribute.getID());
                        ImGui::TextUnformatted(attribute.getName().data());
                        imnodes::EndInputAttribute();
                    } else if (attribute.getType() == Attribute::Type::Out) {
                        imnodes::BeginOutputAttribute(attribute.getID());
                        ImGui::TextUnformatted(attribute.getName().data());
                        imnodes::EndOutputAttribute();
                    }
                }

                imnodes::EndNode();
            }

            for (const auto &link : this->m_links)
                imnodes::Link(link.getID(), link.getFromID(), link.getToID());

            imnodes::EndNodeEditor();

            {
                int from, to;
                if (imnodes::IsLinkCreated(&from, &to)) {
                    auto newLink = this->m_links.emplace_back(from, to);

                    Attribute *fromAttr, *toAttr;
                    for (auto &node : this->m_nodes) {
                        for (auto &attribute : node->getAttributes()) {
                            if (attribute.getID() == from)
                                fromAttr = &attribute;
                            else if (attribute.getID() == to)
                                toAttr = &attribute;
                        }
                    }

                    if (fromAttr == nullptr || toAttr == nullptr)
                        throw std::runtime_error("Invalid node link");

                    fromAttr->setConnectedAttribute(newLink.getID(), toAttr);
                    toAttr->setConnectedAttribute(newLink.getID(), fromAttr);
                }
            }


            {
                int id;
                if (imnodes::IsLinkDestroyed(&id)) {
                    eraseLink(id);
                }
            }

            {
                const int selectedLinkCount = imnodes::NumSelectedLinks();
                if (selectedLinkCount > 0 && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Delete))) {
                    static std::vector<int> selectedLinks;
                    selectedLinks.resize(static_cast<size_t>(selectedLinkCount));
                    imnodes::GetSelectedLinks(selectedLinks.data());
                    for (const int id : selectedLinks) {
                        eraseLink(id);
                    }
                }
            }

            {
                const int selectedNodeCount = imnodes::NumSelectedNodes();
                if (selectedNodeCount > 0 && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Delete))) {
                    static std::vector<int> selectedNodes;
                    selectedNodes.resize(static_cast<size_t>(selectedNodeCount));
                    imnodes::GetSelectedNodes(selectedNodes.data());
                    for (const int id : selectedNodes) {
                        auto node = std::find_if(this->m_nodes.begin(), this->m_nodes.end(), [&id](auto node){ return node->getID() == id; });

                        for (auto &attr : (*node)->getAttributes()) {
                            eraseLink(attr.getConnectedLinkID());
                        }

                        if ((*node)->isEndNode()) {
                            std::erase_if(this->m_endNodes, [&id](auto node){ return node->getID() == id; });
                        }

                        delete *node;

                        this->m_nodes.erase(node);
                    }
                }
            }

            this->processNodes();

        }
        ImGui::End();
    }

    void ViewDataProcessor::drawMenu() {

    }

}