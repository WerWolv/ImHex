#include "content/views/view_data_processor.hpp"

#include <hex/helpers/file.hpp>

#include <hex/providers/provider.hpp>
#include <hex/helpers/project_file_handler.hpp>

#include <imnodes.h>
#include <nlohmann/json.hpp>

namespace hex::plugin::builtin {

    ViewDataProcessor::ViewDataProcessor() : View("hex.builtin.view.data_processor.name") {
        EventManager::subscribe<RequestChangeTheme>(this, [](u32 theme) {
            switch (theme) {
                default:
                case 1: /* Dark theme */
                    ImNodes::StyleColorsDark();
                    break;
                case 2: /* Light theme */
                    ImNodes::StyleColorsLight();
                    break;
                case 3: /* Classic theme */
                    ImNodes::StyleColorsClassic();
                    break;
            }

            ImNodes::GetStyle().Flags = ImNodesStyleFlags_NodeOutline | ImNodesStyleFlags_GridLines;
        });

        EventManager::subscribe<EventProjectFileStore>(this, [this] {
            ProjectFile::setDataProcessorContent(this->saveNodes());
        });

        EventManager::subscribe<EventProjectFileLoad>(this, [this] {
            try {
                this->loadNodes(ProjectFile::getDataProcessorContent());
            } catch (nlohmann::json::exception &e) {

            }
        });

        EventManager::subscribe<EventFileLoaded>(this, [this](const fs::path &path){
            for (auto &node : this->m_nodes) {
                node->setCurrentOverlay(nullptr);
            }
            this->m_dataOverlays.clear();
        });

        ContentRegistry::Interface::addMenuItem("hex.builtin.menu.file", 3000, [&, this] {
            if (ImGui::MenuItem("hex.builtin.view.data_processor.menu.file.load_processor"_lang)) {
                hex::openFileBrowser("hex.builtin.view.data_processor.menu.file.load_processor"_lang, DialogMode::Open, { { "hex.builtin.view.data_processor.name"_lang, "hexnode"} }, [this](const fs::path &path){
                    File file(path, File::Mode::Read);
                    if (file.isValid())
                        this->loadNodes(file.readString());
                });
            }

            if (ImGui::MenuItem("hex.builtin.view.data_processor.menu.file.save_processor"_lang, nullptr, false, !this->m_nodes.empty())) {
                hex::openFileBrowser("hex.builtin.view.data_processor.menu.file.save_processor"_lang, DialogMode::Save, { { "hex.builtin.view.data_processor.name"_lang, "hexnode"} }, [this](const fs::path &path){
                    File file(path, File::Mode::Create);
                    if (file.isValid())
                        file.write(this->saveNodes());
                });

            }
        });
    }

    ViewDataProcessor::~ViewDataProcessor() {
        for (auto &node : this->m_nodes)
            delete node;

        EventManager::unsubscribe<RequestChangeTheme>(this);
        EventManager::unsubscribe<EventFileLoaded>(this);
        EventManager::unsubscribe<EventProjectFileStore>(this);
        EventManager::unsubscribe<EventProjectFileLoad>(this);
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

        ProjectFile::markDirty();
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

        ProjectFile::markDirty();
    }

    void ViewDataProcessor::processNodes() {
        if (this->m_dataOverlays.size() != this->m_endNodes.size()) {
            for (auto overlay : this->m_dataOverlays)
                ImHexApi::Provider::get()->deleteOverlay(overlay);
            this->m_dataOverlays.clear();

            for (u32 i = 0; i < this->m_endNodes.size(); i++)
                this->m_dataOverlays.push_back(ImHexApi::Provider::get()->newOverlay());

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
                ImHexApi::Provider::get()->deleteOverlay(overlay);
            this->m_dataOverlays.clear();

        } catch (std::runtime_error &e) {
            printf("Node implementation bug! %s\n", e.what());
        }

    }

    void ViewDataProcessor::drawContent() {
        if (ImGui::Begin(View::toWindowName("hex.builtin.view.data_processor.name").c_str(), &this->getWindowOpenState(), ImGuiWindowFlags_NoCollapse)) {

            if (ImGui::IsMouseReleased(ImGuiMouseButton_Right) && ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows)) {
                ImNodes::ClearNodeSelection();
                ImNodes::ClearLinkSelection();

                this->m_rightClickedCoords = ImGui::GetMousePos();

                if (ImNodes::IsNodeHovered(&this->m_rightClickedId))
                    ImGui::OpenPopup("Node Menu");
                else if (ImNodes::IsLinkHovered(&this->m_rightClickedId))
                    ImGui::OpenPopup("Link Menu");
                else
                    ImGui::OpenPopup("Context Menu");
            }

            if (ImGui::BeginPopup("Context Menu")) {
                dp::Node *node = nullptr;

                if (ImNodes::NumSelectedNodes() > 0 || ImNodes::NumSelectedLinks() > 0) {
                    if (ImGui::MenuItem("hex.builtin.view.data_processor.name"_lang)) {
                        std::vector<int> ids;
                        ids.resize(ImNodes::NumSelectedNodes());
                        ImNodes::GetSelectedNodes(ids.data());

                        this->eraseNodes(ids);
                        ImNodes::ClearNodeSelection();

                        ids.resize(ImNodes::NumSelectedLinks());
                        ImNodes::GetSelectedLinks(ids.data());

                        for (auto id : ids)
                            this->eraseLink(id);
                        ImNodes::ClearLinkSelection();
                    }
                }

                for (const auto &[unlocalizedCategory, unlocalizedName, function] : ContentRegistry::DataProcessorNode::getEntries()) {
                    if (unlocalizedCategory.empty() && unlocalizedName.empty()) {
                        ImGui::Separator();
                    } else if (unlocalizedCategory.empty()) {
                        if (ImGui::MenuItem(LangEntry(unlocalizedName))) {
                            node = function();
                            ProjectFile::markDirty();
                        }
                    } else {
                        if (ImGui::BeginMenu(LangEntry(unlocalizedCategory))) {
                            if (ImGui::MenuItem(LangEntry(unlocalizedName))) {
                                node = function();
                                ProjectFile::markDirty();
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

                    ImNodes::SetNodeScreenSpacePos(node->getID(), this->m_rightClickedCoords);
                }

                ImGui::EndPopup();
            }

            if (ImGui::BeginPopup("Node Menu")) {
                if (ImGui::MenuItem("hex.builtin.view.data_processor.menu.remove_node"_lang))
                    this->eraseNodes({ this->m_rightClickedId });

                ImGui::EndPopup();
            }

            if (ImGui::BeginPopup("Link Menu")) {
                if (ImGui::MenuItem("hex.builtin.view.data_processor.menu.remove_link"_lang))
                    this->eraseLink(this->m_rightClickedId);

                ImGui::EndPopup();
            }

            {
                int nodeId;
                if (ImNodes::IsNodeHovered(&nodeId) && this->m_currNodeError.has_value() && this->m_currNodeError->first->getID() == nodeId) {
                    ImGui::BeginTooltip();
                    ImGui::TextUnformatted("hex.common.error"_lang);
                    ImGui::Separator();
                    ImGui::TextUnformatted(this->m_currNodeError->second.c_str());
                    ImGui::EndTooltip();
                }
            }

            ImNodes::BeginNodeEditor();

            for (auto& node : this->m_nodes) {
                const bool hasError = this->m_currNodeError.has_value() && this->m_currNodeError->first == node;

                if (hasError)
                    ImNodes::PushColorStyle(ImNodesCol_NodeOutline, 0xFF0000FF);

                ImNodes::BeginNode(node->getID());

                ImNodes::BeginNodeTitleBar();
                ImGui::TextUnformatted(LangEntry(node->getUnlocalizedTitle()));
                ImNodes::EndNodeTitleBar();

                node->drawNode();

                for (auto& attribute : node->getAttributes()) {
                    ImNodesPinShape pinShape;

                    switch (attribute.getType()) {
                        case dp::Attribute::Type::Integer: pinShape = ImNodesPinShape_Circle; break;
                        case dp::Attribute::Type::Float: pinShape = ImNodesPinShape_Triangle; break;
                        case dp::Attribute::Type::Buffer: pinShape = ImNodesPinShape_Quad; break;
                    }

                    if (attribute.getIOType() == dp::Attribute::IOType::In) {
                        ImNodes::BeginInputAttribute(attribute.getID(), pinShape);
                        ImGui::TextUnformatted(LangEntry(attribute.getUnlocalizedName()));
                        ImNodes::EndInputAttribute();
                    } else if (attribute.getIOType() == dp::Attribute::IOType::Out) {
                        ImNodes::BeginOutputAttribute(attribute.getID(), ImNodesPinShape(pinShape + 1));
                        ImGui::TextUnformatted(LangEntry(attribute.getUnlocalizedName()));
                        ImNodes::EndOutputAttribute();
                    }
                }

                ImNodes::EndNode();

                if (hasError)
                    ImNodes::PopColorStyle();
            }

            for (const auto &link : this->m_links)
                ImNodes::Link(link.getID(), link.getFromID(), link.getToID());

            ImNodes::MiniMap(0.2F, ImNodesMiniMapLocation_BottomRight);

            ImNodes::EndNodeEditor();

            {
                int linkId;
                if (ImNodes::IsLinkDestroyed(&linkId)) {
                    this->eraseLink(linkId);
                }
            }

            {
                int from, to;
                if (ImNodes::IsLinkCreated(&from, &to)) {

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
                const int selectedLinkCount = ImNodes::NumSelectedLinks();
                if (selectedLinkCount > 0 && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Delete))) {
                    static std::vector<int> selectedLinks;
                    selectedLinks.resize(static_cast<size_t>(selectedLinkCount));
                    ImNodes::GetSelectedLinks(selectedLinks.data());

                    for (const int id : selectedLinks) {
                        eraseLink(id);
                    }

                }
            }

            {
                const int selectedNodeCount = ImNodes::NumSelectedNodes();
                if (selectedNodeCount > 0 && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Delete))) {
                    static std::vector<int> selectedNodes;
                    selectedNodes.resize(static_cast<size_t>(selectedNodeCount));
                    ImNodes::GetSelectedNodes(selectedNodes.data());

                    this->eraseNodes(selectedNodes);

                }
            }

            this->processNodes();

        }
        ImGui::End();
    }

    std::string ViewDataProcessor::saveNodes() {
        using json = nlohmann::json;
        json output;

        output["nodes"] = json::object();
        for (auto &node : this->m_nodes) {
            auto id = node->getID();
            auto &currNodeOutput = output["nodes"][std::to_string(id)];
            auto pos = ImNodes::GetNodeGridSpacePos(id);

            currNodeOutput["type"] = node->getUnlocalizedName();
            currNodeOutput["pos"] = { { "x", pos.x }, { "y", pos.y } };
            currNodeOutput["attrs"] = json::array();
            currNodeOutput["id"] = id;

            json nodeData;
            node->store(nodeData);
            currNodeOutput["data"] = nodeData;

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

    void ViewDataProcessor::loadNodes(const std::string &data) {
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
            ImNodes::SetNodeGridSpacePos(nodeId, ImVec2(node["pos"]["x"], node["pos"]["y"]));
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