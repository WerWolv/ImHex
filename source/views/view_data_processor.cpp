#include "views/view_data_processor.hpp"

#include <hex/providers/provider.hpp>
#include <helpers/project_file_handler.hpp>

#include <imnodes.h>
#include <nlohmann/json.hpp>

namespace hex {

    ViewDataProcessor::ViewDataProcessor() : View("hex.view.data_processor.name") {
        imnodes::Initialize();
        imnodes::PushAttributeFlag(imnodes::AttributeFlags_EnableLinkDetachWithDragClick);
        imnodes::PushAttributeFlag(imnodes::AttributeFlags_EnableLinkCreationOnSnap);

        {
            static bool always = true;
            imnodes::IO& io = imnodes::GetIO();
            io.link_detach_with_modifier_click.modifier = &always;
        }

        EventManager::subscribe<EventSettingsChanged>(this, [] {
            auto theme = ContentRegistry::Settings::getSetting("hex.builtin.setting.interface", "hex.builtin.setting.interface.color");

            if (theme.is_number()) {
                switch (static_cast<int>(theme)) {
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

        EventManager::subscribe<EventProjectFileStore>(this, [this] {
            ProjectFile::setDataProcessorContent(this->saveNodes());
        });

        EventManager::subscribe<EventProjectFileLoad>(this, [this] {
            this->loadNodes(ProjectFile::getDataProcessorContent());
        });

        EventManager::subscribe<EventFileLoaded>(this, [this](const std::string &path){
            for (auto &node : this->m_nodes) {
                node->setCurrentOverlay(nullptr);
            }
            this->m_dataOverlays.clear();
        });
    }

    ViewDataProcessor::~ViewDataProcessor() {
        for (auto &node : this->m_nodes)
            delete node;

        EventManager::unsubscribe<EventSettingsChanged>(this);
        EventManager::unsubscribe<EventFileLoaded>(this);
        EventManager::unsubscribe<EventProjectFileStore>(this);
        EventManager::unsubscribe<EventProjectFileLoad>(this);

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

                for (auto &node : this->m_nodes)
                    node->resetProcessedInputs();

                endNode->process();
            }
        } catch (dp::Node::NodeError &e) {
            this->m_currNodeError = e;

            for (auto overlay : this->m_dataOverlays)
                SharedData::currentProvider->deleteOverlay(overlay);
            this->m_dataOverlays.clear();

        } catch (std::runtime_error &e) {
            printf("Node implementation bug! %s\n", e.what());
        }

    }

    void ViewDataProcessor::drawContent() {
        if (ImGui::Begin(View::toWindowName("hex.view.data_processor.name").c_str(), &this->getWindowOpenState(), ImGuiWindowFlags_NoCollapse)) {

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

                for (const auto &[unlocalizedCategory, unlocalizedName, function] : ContentRegistry::DataProcessorNode::getEntries()) {
                    if (unlocalizedCategory.empty() && unlocalizedName.empty()) {
                        ImGui::Separator();
                    } else if (unlocalizedCategory.empty()) {
                        if (ImGui::MenuItem(LangEntry(unlocalizedName))) {
                            node = function();
                        }
                    } else {
                        if (ImGui::BeginMenu(LangEntry(unlocalizedCategory))) {
                            if (ImGui::MenuItem(LangEntry(unlocalizedName))) {
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
                ImGui::TextUnformatted(LangEntry(node->getUnlocalizedTitle()));
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

    std::string ViewDataProcessor::saveNodes() {
        using json = nlohmann::json;
        json output;

        output["nodes"] = json::object();
        for (auto &node : this->m_nodes) {
            auto id = node->getID();
            auto &currNodeOutput = output["nodes"][std::to_string(id)];
            auto pos = imnodes::GetNodeGridSpacePos(id);

            currNodeOutput["type"] = node->getUnlocalizedName();
            currNodeOutput["pos"] = { { "x", pos.x }, { "y", pos.y } };
            currNodeOutput["attrs"] = json::array();
            currNodeOutput["id"] = id;

            currNodeOutput["data"] = node->store();

            u32 attrIndex = 0;
            for (auto &attr : node->getAttributes()) {
                currNodeOutput["attrs"][attrIndex] = attr.getID();
                attrIndex++;
            }
        }

        output["links"] = json::object();
        for (auto &link : this->m_links) {
            auto id = link.getID();
            auto &currOutput = output["links"][std::to_string(id)];

            currOutput["id"] = id;
            currOutput["from"] = link.getFromID();
            currOutput["to"] = link.getToID();
        }

        return output.dump();
    }

    void ViewDataProcessor::loadNodes(std::string_view data) {
        using json = nlohmann::json;

        json input = json::parse(data);

        u32 maxNodeId = 0;
        u32 maxAttrId = 0;
        u32 maxLinkId = 0;

        for (auto &node : this->m_nodes)
            delete node;

        this->m_nodes.clear();
        this->m_endNodes.clear();
        this->m_links.clear();

        auto &nodeEntries = ContentRegistry::DataProcessorNode::getEntries();
        for (auto &node : input["nodes"]) {
            dp::Node *newNode = nullptr;
            for (auto &entry : nodeEntries) {
                if (entry.name == node["type"])
                    newNode = entry.creatorFunction();
            }

            if (newNode == nullptr)
                continue;

            u32 nodeId = node["id"];
            maxNodeId = std::max(nodeId, maxNodeId);

            newNode->setID(nodeId);

            bool hasOutput = false;
            bool hasInput = false;
            u32 attrIndex = 0;
            for (auto &attr : newNode->getAttributes()) {
                if (attr.getIOType() == dp::Attribute::IOType::Out)
                    hasOutput = true;

                if (attr.getIOType() == dp::Attribute::IOType::In)
                    hasInput = true;

                u32 attrId = node["attrs"][attrIndex];
                maxAttrId = std::max(attrId, maxAttrId);

                attr.setID(attrId);
                attrIndex++;
            }

            if (!node["data"].is_null())
                newNode->load(node["data"]);

            if (hasInput && !hasOutput)
                this->m_endNodes.push_back(newNode);

            this->m_nodes.push_back(newNode);
            imnodes::SetNodeGridSpacePos(nodeId, ImVec2(node["pos"]["x"], node["pos"]["y"]));
        }

        for (auto &link : input["links"]) {
            dp::Link newLink(link["from"], link["to"]);

            u32 linkId = link["id"];
            maxLinkId = std::max(linkId, maxLinkId);

            newLink.setID(linkId);
            this->m_links.push_back(newLink);

            dp::Attribute *fromAttr, *toAttr;
            for (auto &node : this->m_nodes) {
                for (auto &attribute : node->getAttributes()) {
                    if (attribute.getID() == newLink.getFromID())
                        fromAttr = &attribute;
                    else if (attribute.getID() == newLink.getToID())
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

            fromAttr->addConnectedAttribute(newLink.getID(), toAttr);
            toAttr->addConnectedAttribute(newLink.getID(), fromAttr);
        }

        SharedData::dataProcessorNodeIdCounter = maxNodeId + 1;
        SharedData::dataProcessorAttrIdCounter = maxAttrId + 1;
        SharedData::dataProcessorLinkIdCounter = maxLinkId + 1;
    }

}