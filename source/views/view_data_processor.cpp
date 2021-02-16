#include "views/view_data_processor.hpp"

#include <hex/providers/provider.hpp>

#include <imnodes.h>

namespace hex {

    ViewDataProcessor::ViewDataProcessor() : View("hex.view.data_processor.name"_lang) {
        imnodes::Initialize();
        imnodes::PushAttributeFlag(imnodes::AttributeFlags_EnableLinkDetachWithDragClick);
        imnodes::PushAttributeFlag(imnodes::AttributeFlags_EnableLinkCreationOnSnap);

        {
            static bool always = true;
            imnodes::IO& io = imnodes::GetIO();
            io.link_detach_with_modifier_click.modifier = &always;
        }

        View::subscribeEvent(Events::SettingsChanged, [](auto) {
            auto theme = ContentRegistry::Settings::getSetting("hex.builtin.setting.interface", "hex.builtin.setting.interface.color");

            if (theme.has_value()) {
                switch (static_cast<int>(theme.value())) {
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
            }
        });

        View::subscribeEvent(Events::FileLoaded, [this](auto) {
            for (auto &node : this->m_nodes) {
                node->setCurrentOverlay(nullptr);
            }
            this->m_dataOverlays.clear();
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

            u32 overlayIndex = 0;
            for (auto endNode : this->m_endNodes) {
                endNode->setCurrentOverlay(this->m_dataOverlays[overlayIndex]);
                overlayIndex++;
            }
        }

        this->m_currNodeError.reset();

        try {
            for (auto &endNode : this->m_endNodes) {
                endNode->resetOutputData();
                endNode->process();
            }
        } catch (dp::Node::NodeError &e) {
            this->m_currNodeError = e;
        } catch (std::runtime_error &e) {
            printf("Node implementation bug! %s\n", e.what());
        }

    }

    void ViewDataProcessor::drawContent() {
        if (ImGui::Begin("hex.view.data_processor.name"_lang, &this->getWindowOpenState(), ImGuiWindowFlags_NoCollapse)) {

            if (ImGui::IsMouseReleased(ImGuiMouseButton_Right) && ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows)) {
                imnodes::ClearNodeSelection();
                imnodes::ClearLinkSelection();

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
                    if (ImGui::MenuItem("hex.view.data_processor.name"_lang)) {
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
                    bool hasInput = false;
                    for (auto &attr : node->getAttributes()) {
                        if (attr.getIOType() == dp::Attribute::IOType::Out)
                            hasOutput = true;

                        if (attr.getIOType() == dp::Attribute::IOType::In)
                            hasInput = true;
                    }

                    if (hasInput && !hasOutput)
                        this->m_endNodes.push_back(node);

                    imnodes::SetNodeScreenSpacePos(node->getID(), this->m_rightClickedCoords);
                }

                ImGui::EndPopup();
            }

            if (ImGui::BeginPopup("Node Menu")) {
                if (ImGui::MenuItem("hex.view.data_processor.menu.remove_node"_lang))
                    this->eraseNodes({ this->m_rightClickedId });

                ImGui::EndPopup();
            }

            if (ImGui::BeginPopup("Link Menu")) {
                if (ImGui::MenuItem("hex.view.data_processor.menu.remove_link"_lang))
                    this->eraseLink(this->m_rightClickedId);

                ImGui::EndPopup();
            }

            {
                int nodeId;
                if (imnodes::IsNodeHovered(&nodeId) && this->m_currNodeError.has_value() && this->m_currNodeError->first->getID() == nodeId) {
                    ImGui::BeginTooltip();
                    ImGui::TextUnformatted("hex.common.error"_lang);
                    ImGui::Separator();
                    ImGui::TextUnformatted(this->m_currNodeError->second.c_str());
                    ImGui::EndTooltip();
                }
            }

            imnodes::BeginNodeEditor();

            for (auto& node : this->m_nodes) {
                const bool hasError = this->m_currNodeError.has_value() && this->m_currNodeError->first == node;

                if (hasError)
                    imnodes::PushColorStyle(imnodes::ColorStyle_NodeOutline, 0xFF0000FF);

                imnodes::BeginNode(node->getID());

                imnodes::BeginNodeTitleBar();
                ImGui::TextUnformatted(LangEntry(node->getUnlocalizedName()));
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
                        ImGui::TextUnformatted(LangEntry(attribute.getUnlocalizedName()));
                        imnodes::EndInputAttribute();
                    } else if (attribute.getIOType() == dp::Attribute::IOType::Out) {
                        imnodes::BeginOutputAttribute(attribute.getID(), imnodes::PinShape(pinShape + 1));
                        ImGui::TextUnformatted(LangEntry(attribute.getUnlocalizedName()));
                        imnodes::EndOutputAttribute();
                    }
                }

                imnodes::EndNode();

                if (hasError)
                    imnodes::PopColorStyle();
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