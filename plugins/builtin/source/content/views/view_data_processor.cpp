#include "content/views/view_data_processor.hpp"

#include <hex/api/content_registry.hpp>

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

        EventManager::subscribe<EventFileLoaded>(this, [this](const std::fs::path &path) {
            hex::unused(path);

            for (auto &node : this->m_nodes) {
                node->setCurrentOverlay(nullptr);
            }
            this->m_dataOverlays.clear();
        });

        EventManager::subscribe<EventDataChanged>(this, [this] {
            this->processNodes();
        });

        ContentRegistry::Interface::addMenuItem("hex.builtin.menu.file", 3000, [&, this] {
            bool providerValid = ImHexApi::Provider::isValid();

            if (ImGui::MenuItem("hex.builtin.view.data_processor.menu.file.load_processor"_lang, nullptr, false, providerValid)) {
                fs::openFileBrowser(fs::DialogMode::Open, { {"hex.builtin.view.data_processor.name"_lang, "hexnode"} },
                    [this](const std::fs::path &path) {
                        fs::File file(path, fs::File::Mode::Read);
                        if (file.isValid())
                            this->loadNodes(file.readString());
                    });
            }

            if (ImGui::MenuItem("hex.builtin.view.data_processor.menu.file.save_processor"_lang, nullptr, false, !this->m_nodes.empty() && providerValid)) {
                fs::openFileBrowser(fs::DialogMode::Save, { {"hex.builtin.view.data_processor.name"_lang, "hexnode"} },
                    [this](const std::fs::path &path) {
                        fs::File file(path, fs::File::Mode::Create);
                        if (file.isValid())
                            file.write(this->saveNodes());
                    });
            }
        });

        ContentRegistry::FileHandler::add({ ".hexnode" }, [this](const auto &path) {
            fs::File file(path, fs::File::Mode::Read);
            if (!file.isValid()) return false;

            this->loadNodes(file.readString());

            return true;
        });
    }

    ViewDataProcessor::~ViewDataProcessor() {
        for (auto &node : this->m_nodes)
            delete node;

        EventManager::unsubscribe<RequestChangeTheme>(this);
        EventManager::unsubscribe<EventFileLoaded>(this);
        EventManager::unsubscribe<EventProjectFileStore>(this);
        EventManager::unsubscribe<EventProjectFileLoad>(this);
        EventManager::unsubscribe<EventDataChanged>(this);
    }


    void ViewDataProcessor::eraseLink(u32 id) {
        auto link = std::find_if(this->m_links.begin(), this->m_links.end(), [&id](auto link) { return link.getId() == id; });

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
        for (u32 id : ids) {
            auto node = std::find_if(this->m_nodes.begin(), this->m_nodes.end(), [&id](auto node) { return node->getId() == id; });

            for (auto &attr : (*node)->getAttributes()) {
                std::vector<u32> linksToRemove;
                for (auto &[linkId, connectedAttr] : attr.getConnectedAttributes())
                    linksToRemove.push_back(linkId);

                for (auto linkId : linksToRemove)
                    eraseLink(linkId);
            }
        }

        for (u32 id : ids) {
            auto node = std::find_if(this->m_nodes.begin(), this->m_nodes.end(), [&id](auto node) { return node->getId() == id; });

            std::erase_if(this->m_endNodes, [&id](auto node) { return node->getId() == id; });

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
                    bool hasInput  = false;
                    for (auto &attr : node->getAttributes()) {
                        if (attr.getIOType() == dp::Attribute::IOType::Out)
                            hasOutput = true;

                        if (attr.getIOType() == dp::Attribute::IOType::In)
                            hasInput = true;
                    }

                    if (hasInput && !hasOutput)
                        this->m_endNodes.push_back(node);

                    ImNodes::SetNodeScreenSpacePos(node->getId(), this->m_rightClickedCoords);
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
                if (ImNodes::IsNodeHovered(&nodeId) && this->m_currNodeError.has_value() && this->m_currNodeError->first->getId() == static_cast<u32>(nodeId)) {
                    ImGui::BeginTooltip();
                    ImGui::TextUnformatted("hex.builtin.common.error"_lang);
                    ImGui::Separator();
                    ImGui::TextUnformatted(this->m_currNodeError->second.c_str());
                    ImGui::EndTooltip();
                }
            }

            if (ImGui::BeginChild("##node_editor", ImGui::GetContentRegionAvail() - ImVec2(0, ImGui::GetTextLineHeightWithSpacing() * 1.3))) {
                ImNodes::BeginNodeEditor();

                for (auto &node : this->m_nodes) {
                    const bool hasError = this->m_currNodeError.has_value() && this->m_currNodeError->first == node;

                    if (hasError)
                        ImNodes::PushColorStyle(ImNodesCol_NodeOutline, 0xFF0000FF);

                    ImNodes::BeginNode(node->getId());

                    ImNodes::BeginNodeTitleBar();
                    ImGui::TextUnformatted(LangEntry(node->getUnlocalizedTitle()));
                    ImNodes::EndNodeTitleBar();

                    node->drawNode();

                    for (auto &attribute : node->getAttributes()) {
                        ImNodesPinShape pinShape;

                        switch (attribute.getType()) {
                            default:
                            case dp::Attribute::Type::Integer:
                                pinShape = ImNodesPinShape_Circle;
                                break;
                            case dp::Attribute::Type::Float:
                                pinShape = ImNodesPinShape_Triangle;
                                break;
                            case dp::Attribute::Type::Buffer:
                                pinShape = ImNodesPinShape_Quad;
                                break;
                        }

                        if (attribute.getIOType() == dp::Attribute::IOType::In) {
                            ImNodes::BeginInputAttribute(attribute.getId(), pinShape);
                            ImGui::TextUnformatted(LangEntry(attribute.getUnlocalizedName()));
                            ImNodes::EndInputAttribute();
                        } else if (attribute.getIOType() == dp::Attribute::IOType::Out) {
                            ImNodes::BeginOutputAttribute(attribute.getId(), ImNodesPinShape(pinShape + 1));
                            ImGui::TextUnformatted(LangEntry(attribute.getUnlocalizedName()));
                            ImNodes::EndOutputAttribute();
                        }
                    }

                    ImNodes::EndNode();

                    if (hasError)
                        ImNodes::PopColorStyle();
                }

                for (const auto &link : this->m_links)
                    ImNodes::Link(link.getId(), link.getFromId(), link.getToId());

                ImNodes::MiniMap(0.2F, ImNodesMiniMapLocation_BottomRight);

                if (this->m_nodes.empty())
                    ImGui::TextFormattedCentered("{}", "hex.builtin.view.data_processor.help_text"_lang);

                ImNodes::EndNodeEditor();
            }
            ImGui::EndChild();

            if (ImGui::IconButton(ICON_VS_DEBUG_START, ImGui::GetCustomColorVec4(ImGuiCustomCol_ToolbarGreen)) || this->m_continuousEvaluation)
                this->processNodes();

            ImGui::SameLine();
            ImGui::Checkbox("Continuous evaluation", &this->m_continuousEvaluation);

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
                        dp::Attribute *fromAttr = nullptr, *toAttr = nullptr;
                        for (auto &node : this->m_nodes) {
                            for (auto &attribute : node->getAttributes()) {
                                if (attribute.getId() == static_cast<u32>(from))
                                    fromAttr = &attribute;
                                else if (attribute.getId() == static_cast<u32>(to))
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

                        fromAttr->addConnectedAttribute(newLink.getId(), toAttr);
                        toAttr->addConnectedAttribute(newLink.getId(), fromAttr);
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
        }
        ImGui::End();
    }

    std::string ViewDataProcessor::saveNodes() {
        using json = nlohmann::json;
        json output;

        output["nodes"] = json::object();
        for (auto &node : this->m_nodes) {
            auto id              = node->getId();
            auto &currNodeOutput = output["nodes"][std::to_string(id)];
            auto pos             = ImNodes::GetNodeGridSpacePos(id);

            currNodeOutput["type"] = node->getUnlocalizedName();
            currNodeOutput["pos"]  = {
                {"x",  pos.x},
                { "y", pos.y}
            };
            currNodeOutput["attrs"] = json::array();
            currNodeOutput["id"]    = id;

            json nodeData;
            node->store(nodeData);
            currNodeOutput["data"] = nodeData;

            u32 attrIndex = 0;
            for (auto &attr : node->getAttributes()) {
                currNodeOutput["attrs"][attrIndex] = attr.getId();
                attrIndex++;
            }
        }

        output["links"] = json::object();
        for (auto &link : this->m_links) {
            auto id          = link.getId();
            auto &currOutput = output["links"][std::to_string(id)];

            currOutput["id"]   = id;
            currOutput["from"] = link.getFromId();
            currOutput["to"]   = link.getToId();
        }

        return output.dump();
    }

    void ViewDataProcessor::loadNodes(const std::string &data) {
        if (!ImHexApi::Provider::isValid()) return;

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
            maxNodeId  = std::max(nodeId, maxNodeId);

            newNode->setId(nodeId);

            bool hasOutput = false;
            bool hasInput  = false;
            u32 attrIndex  = 0;
            for (auto &attr : newNode->getAttributes()) {
                if (attr.getIOType() == dp::Attribute::IOType::Out)
                    hasOutput = true;

                if (attr.getIOType() == dp::Attribute::IOType::In)
                    hasInput = true;

                u32 attrId = node["attrs"][attrIndex];
                maxAttrId  = std::max(attrId, maxAttrId);

                attr.setId(attrId);
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
            maxLinkId  = std::max(linkId, maxLinkId);

            newLink.setID(linkId);
            this->m_links.push_back(newLink);

            dp::Attribute *fromAttr = nullptr, *toAttr = nullptr;
            for (auto &node : this->m_nodes) {
                for (auto &attribute : node->getAttributes()) {
                    if (attribute.getId() == newLink.getFromId())
                        fromAttr = &attribute;
                    else if (attribute.getId() == newLink.getToId())
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

            fromAttr->addConnectedAttribute(newLink.getId(), toAttr);
            toAttr->addConnectedAttribute(newLink.getId(), fromAttr);
        }

        dp::Node::setIdCounter(maxNodeId + 1);
        dp::Attribute::setIdCounter(maxAttrId + 1);
        dp::Link::setIdCounter(maxLinkId + 1);

        this->processNodes();
    }

}