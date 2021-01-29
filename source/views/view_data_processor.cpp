#include "views/view_data_processor.hpp"

#include <hex/providers/provider.hpp>

#include <imnodes.h>

namespace hex {

    ViewDataProcessor::ViewDataProcessor() : View("Data Processor") {
        imnodes::Initialize();

        View::subscribeEvent(Events::SettingsChanged, [this](auto) {
            int theme = ContentRegistry::Settings::getSettingsData()["Interface"]["Color theme"];

            switch (theme) {
                default:
                case 0: /* Dark theme */
                    imnodes::StyleColorsDark();
                    break;
                case 1: /* Light theme */
                    imnodes::StyleColorsLight();
                    break;
                case 2: /* Classic theme */
                    imnodes::StyleColorsClassic();
                    break;
            }

            imnodes::GetStyle().flags = imnodes::StyleFlags(imnodes::StyleFlags_NodeOutline | imnodes::StyleFlags_GridLines);
        });
    }

    ViewDataProcessor::~ViewDataProcessor() {
        imnodes::Shutdown();
    }


    auto ViewDataProcessor::eraseLink(u32 id) {
        auto link = std::find_if(this->m_links.begin(), this->m_links.end(), [&id](auto link){ return link.getID() == id; });

        if (link == this->m_links.end())
            return;

        for (auto &node : this->m_nodes) {
            for (auto &attribute : node->getAttributes()) {
                if (attribute.getID() == link->getFromID() || attribute.getID() == link->getToID()) {
                    attribute.removeConnectedAttribute(attribute.getID());
                }
            }
        }

        this->m_links.erase(link);
    }

    void ViewDataProcessor::processNodes() {
        if (this->m_dataOverlays.size() != this->m_endNodes.size()) {
            for (auto overlay : this->m_dataOverlays)
                SharedData::currentProvider->deleteOverlay(overlay);
            this->m_dataOverlays.clear();

            for (u32 i = 0; i < this->m_endNodes.size(); i++)
                this->m_dataOverlays.push_back(SharedData::currentProvider->newOverlay());
        }

        u32 overlayIndex = 0;
        for (auto &endNode : this->m_endNodes) {
            (void)endNode->process(this->m_dataOverlays[overlayIndex]);
            overlayIndex++;
        }
    }

    void ViewDataProcessor::drawContent() {
        if (ImGui::Begin("Data Processor", &this->getWindowOpenState(), ImGuiWindowFlags_NoCollapse)) {
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) ImGui::OpenPopup("Add Node");

            if (ImGui::BeginPopup("Add Node")) {
                dp::Node *node = nullptr;

                for (const auto &[name, function] : ContentRegistry::DataProcessorNode::getEntries()) {
                    if (ImGui::MenuItem(name.c_str())) {
                        node = function();
                    }
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
                    if (attribute.getType() == dp::Attribute::Type::In) {
                        imnodes::BeginInputAttribute(attribute.getID());
                        ImGui::TextUnformatted(attribute.getName().data());
                        imnodes::EndInputAttribute();
                    } else if (attribute.getType() == dp::Attribute::Type::Out) {
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

                    dp::Attribute *fromAttr, *toAttr;
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

                    fromAttr->addConnectedAttribute(newLink.getID(), toAttr);
                    toAttr->addConnectedAttribute(newLink.getID(), fromAttr);
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
                            std::vector<u32> linksToRemove;
                            for (auto &[linkId, connectedAttr] : attr.getConnectedAttributes())
                                linksToRemove.push_back(linkId);

                            for (auto linkId : linksToRemove)
                                eraseLink(linkId);
                        }
                    }

                    for (const int id :selectedNodes) {
                        auto node = std::find_if(this->m_nodes.begin(), this->m_nodes.end(), [&id](auto node){ return node->getID() == id; });

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