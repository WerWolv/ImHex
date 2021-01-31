#include "views/view_data_processor.hpp"

#include <hex/providers/provider.hpp>

#include <imnodes.h>

namespace hex {

    ViewDataProcessor::ViewDataProcessor() : View("Data Processor") {
        imnodes::Initialize();
        imnodes::PushAttributeFlag(imnodes::AttributeFlags_EnableLinkDetachWithDragClick);
        imnodes::PushAttributeFlag(imnodes::AttributeFlags_EnableLinkCreationOnSnap);

        {
            static bool always = true;
            imnodes::IO& io = imnodes::GetIO();
            io.link_detach_with_modifier_click.modifier = &always;
        }

        View::subscribeEvent(Events::SettingsChanged, [](auto) {
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
        for (auto &node : this->m_nodes)
            delete node;

        imnodes::PopAttributeFlag();
        imnodes::PopAttributeFlag();
        imnodes::Shutdown();
    }


    void ViewDataProcessor::eraseLink(u32 id) {
        auto link = std::find_if(this->m_links.begin(), this->m_links.end(), [&id](auto link){ return link.getID() == id; });

        if (link == this->m_links.end())
            return;

        for (auto &node : this->m_nodes) {
            for (auto &attribute : node->getAttributes()) {
                attribute.removeConnectedAttribute(id);
            }
        }

        this->m_links.erase(link);
    }

    void ViewDataProcessor::eraseNodes(const std::vector<int> &ids) {
        for (const int id : ids) {
            auto node = std::find_if(this->m_nodes.begin(), this->m_nodes.end(), [&id](auto node){ return node->getID() == id; });

            for (auto &attr : (*node)->getAttributes()) {
                std::vector<u32> linksToRemove;
                for (auto &[linkId, connectedAttr] : attr.getConnectedAttributes())
                    linksToRemove.push_back(linkId);

                for (auto linkId : linksToRemove)
                    eraseLink(linkId);
            }
        }

        for (const int id : ids) {
            auto node = std::find_if(this->m_nodes.begin(), this->m_nodes.end(), [&id](auto node){ return node->getID() == id; });

            std::erase_if(this->m_endNodes, [&id](auto node){ return node->getID() == id; });

            delete *node;

            this->m_nodes.erase(node);
        }
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

            if (ImGui::IsMouseReleased(ImGuiMouseButton_Right) && ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows)) {
                this->m_rightClickedCoords = ImGui::GetMousePos();

                if (imnodes::IsNodeHovered(&this->m_rightClickedId))
                    ImGui::OpenPopup("Node Menu");
                else if (imnodes::IsLinkHovered(&this->m_rightClickedId))
                    ImGui::OpenPopup("Link Menu");
                else
                    ImGui::OpenPopup("Context Menu");
            }

            if (ImGui::BeginPopup("Context Menu")) {
                dp::Node *node = nullptr;

                if (imnodes::NumSelectedNodes() > 0 || imnodes::NumSelectedLinks() > 0) {
                    if (ImGui::MenuItem("Remove selected")) {
                        std::vector<int> ids;
                        ids.resize(imnodes::NumSelectedNodes());
                        imnodes::GetSelectedNodes(ids.data());

                        this->eraseNodes(ids);
                        imnodes::ClearNodeSelection();

                        ids.resize(imnodes::NumSelectedLinks());
                        imnodes::GetSelectedLinks(ids.data());

                        for (auto id : ids)
                            this->eraseLink(id);
                        imnodes::ClearLinkSelection();
                    }
                }

                for (const auto &[category, name, function] : ContentRegistry::DataProcessorNode::getEntries()) {
                    if (category.empty() && name.empty()) {
                        ImGui::Separator();
                    } else if (category.empty()) {
                        if (ImGui::MenuItem(name.c_str())) {
                            node = function();
                        }
                    } else {
                        if (ImGui::BeginMenu(category.c_str())) {
                            if (ImGui::MenuItem(name.c_str())) {
                                node = function();
                            }
                            ImGui::EndMenu();
                        }
                    }
                }

                if (node != nullptr) {
                    this->m_nodes.push_back(node);

                    bool hasOutput = false;
                    for (auto &attr : node->getAttributes()) {
                        if (attr.getIOType() == dp::Attribute::IOType::Out)
                            hasOutput = true;
                    }

                    if (!hasOutput)
                        this->m_endNodes.push_back(node);

                    imnodes::SetNodeScreenSpacePos(node->getID(), this->m_rightClickedCoords);
                }

                ImGui::EndPopup();
            }

            if (ImGui::BeginPopup("Node Menu")) {
                if (ImGui::MenuItem("Remove Node"))
                    this->eraseNodes({ this->m_rightClickedId });

                ImGui::EndPopup();
            }

            if (ImGui::BeginPopup("Link Menu")) {
                if (ImGui::MenuItem("Remove Link"))
                    this->eraseLink(this->m_rightClickedId);

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
                    imnodes::PinShape pinShape;

                    switch (attribute.getType()) {
                        case dp::Attribute::Type::Integer: pinShape = imnodes::PinShape_Circle; break;
                        case dp::Attribute::Type::Float: pinShape = imnodes::PinShape_Triangle; break;
                        case dp::Attribute::Type::Buffer: pinShape = imnodes::PinShape_Quad; break;
                    }

                    if (attribute.getIOType() == dp::Attribute::IOType::In) {
                        imnodes::BeginInputAttribute(attribute.getID(), pinShape);
                        ImGui::TextUnformatted(attribute.getName().data());
                        imnodes::EndInputAttribute();
                    } else if (attribute.getIOType() == dp::Attribute::IOType::Out) {
                        imnodes::BeginOutputAttribute(attribute.getID(), imnodes::PinShape(pinShape + 1));
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
                int linkId;
                if (imnodes::IsLinkDestroyed(&linkId)) {
                    this->eraseLink(linkId);
                }
            }

            {
                int from, to;
                if (imnodes::IsLinkCreated(&from, &to)) {

                    do {
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
                            break;

                        if (fromAttr->getType() != toAttr->getType())
                            break;

                        if (fromAttr->getIOType() == toAttr->getIOType())
                            break;

                        if (!toAttr->getConnectedAttributes().empty())
                            break;

                        auto newLink = this->m_links.emplace_back(from, to);

                        fromAttr->addConnectedAttribute(newLink.getID(), toAttr);
                        toAttr->addConnectedAttribute(newLink.getID(), fromAttr);
                    } while (false);

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

                    this->eraseNodes(selectedNodes);

                }
            }

            this->processNodes();

        }
        ImGui::End();
    }

    void ViewDataProcessor::drawMenu() {

    }

}