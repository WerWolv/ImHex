#include "content/views/view_data_processor.hpp"
#include "content/popups/popup_notification.hpp"

#include <hex/api/content_registry.hpp>

#include <hex/helpers/logger.hpp>

#include <hex/providers/provider.hpp>
#include <hex/api/project_file_manager.hpp>


#include <imnodes.h>
#include <imnodes_internal.h>
#include <nlohmann/json.hpp>

#include <wolv/io/file.hpp>
#include <wolv/utils/guards.hpp>
#include <wolv/utils/core.hpp>

namespace hex::plugin::builtin {

    class NodeCustomInput : public dp::Node {
    public:
        NodeCustomInput() : Node("hex.builtin.nodes.custom.input.header", { dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Integer, "hex.builtin.nodes.common.input") }) { }
        ~NodeCustomInput() override = default;

        void drawNode() override {
            ImGui::PushItemWidth(100_scaled);
            if (ImGui::Combo("##type", &this->m_type, "Integer\0Float\0Buffer\0")) {
                this->setAttributes({
                                            { dp::Attribute(dp::Attribute::IOType::Out, this->getType(), "hex.builtin.nodes.common.input") }
                                    });
            }

            if (ImGui::InputText("##name", this->m_name)) {
                this->setUnlocalizedTitle(this->m_name);
            }

            ImGui::PopItemWidth();
        }

        void setValue(auto value) { this->m_value = std::move(value); }

        const std::string &getName() const { return this->m_name; }
        dp::Attribute::Type getType() const {
            switch (this->m_type) {
                default:
                case 0: return dp::Attribute::Type::Integer;
                case 1: return dp::Attribute::Type::Float;
                case 2: return dp::Attribute::Type::Buffer;
            }
        }

        void process() override {
            std::visit(wolv::util::overloaded {
                    [this](i128 value) { this->setIntegerOnOutput(0, value); },
                    [this](long double value) { this->setFloatOnOutput(0, value); },
                    [this](const std::vector<u8> &value) { this->setBufferOnOutput(0, value); }
            }, this->m_value);
        }

        void store(nlohmann::json &j) const override {
            j = nlohmann::json::object();

            j["name"] = this->m_name;
            j["type"] = this->m_type;
        }

        void load(const nlohmann::json &j) override {
            this->m_name = j.at("name").get<std::string>();
            this->m_type = j.at("type");

            this->setUnlocalizedTitle(this->m_name);
            this->setAttributes({
                                        { dp::Attribute(dp::Attribute::IOType::Out, this->getType(), "hex.builtin.nodes.common.input") }
                                });
        }

    private:
        std::string m_name = LangEntry(this->getUnlocalizedName());
        int m_type = 0;

        std::variant<i128, long double, std::vector<u8>> m_value;
    };

    class NodeCustomOutput : public dp::Node {
    public:
        NodeCustomOutput() : Node("hex.builtin.nodes.custom.output.header", { dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Integer, "hex.builtin.nodes.common.output") }) { }
        ~NodeCustomOutput() override = default;

        void drawNode() override {
            ImGui::PushItemWidth(100_scaled);
            if (ImGui::Combo("##type", &this->m_type, "Integer\0Float\0Buffer\0")) {
                this->setAttributes({
                                            { dp::Attribute(dp::Attribute::IOType::In, this->getType(), "hex.builtin.nodes.common.output") }
                                    });
            }

            if (ImGui::InputText("##name", this->m_name)) {
                this->setUnlocalizedTitle(this->m_name);
            }

            ImGui::PopItemWidth();
        }

        const std::string &getName() const { return this->m_name; }
        dp::Attribute::Type getType() const {
            switch (this->m_type) {
                case 0: return dp::Attribute::Type::Integer;
                case 1: return dp::Attribute::Type::Float;
                case 2: return dp::Attribute::Type::Buffer;
                default: return dp::Attribute::Type::Integer;
            }
        }

        void process() override {
            switch (this->getType()) {
                case dp::Attribute::Type::Integer: this->m_value = this->getIntegerOnInput(0); break;
                case dp::Attribute::Type::Float:   this->m_value = this->getFloatOnInput(0); break;
                case dp::Attribute::Type::Buffer:  this->m_value = this->getBufferOnInput(0); break;
            }
        }

        const auto& getValue() const { return this->m_value; }

        void store(nlohmann::json &j) const override {
            j = nlohmann::json::object();

            j["name"] = this->m_name;
            j["type"] = this->m_type;
        }

        void load(const nlohmann::json &j) override {
            this->m_name = j.at("name").get<std::string>();
            this->m_type = j.at("type");

            this->setUnlocalizedTitle(this->m_name);
            this->setAttributes({
                                        { dp::Attribute(dp::Attribute::IOType::In, this->getType(), "hex.builtin.nodes.common.output") }
                                });
        }

    private:
        std::string m_name = LangEntry(this->getUnlocalizedName());
        int m_type = 0;

        std::variant<i128, long double, std::vector<u8>> m_value;
    };

    class NodeCustom : public dp::Node {
    public:
        explicit NodeCustom(ViewDataProcessor *dataProcessor) : Node("hex.builtin.nodes.custom.custom.header", {}), m_dataProcessor(dataProcessor) { }
        ~NodeCustom() override = default;

        void drawNode() override {
            if (this->m_requiresAttributeUpdate) {
                this->m_requiresAttributeUpdate = false;
                this->setAttributes(this->findAttributes());
            }

            ImGui::PushItemWidth(200_scaled);

            bool editing = false;
            if (this->m_editable) {
                ImGui::InputTextIcon("##name", ICON_VS_SYMBOL_KEY, this->m_name);
                editing = ImGui::IsItemActive();

                if (ImGui::Button("hex.builtin.nodes.custom.custom.edit"_lang, ImVec2(200_scaled, ImGui::GetTextLineHeightWithSpacing()))) {
                    this->m_dataProcessor->getWorkspaceStack().push_back(&this->m_workspace);

                    this->m_requiresAttributeUpdate = true;
                }
            } else {
                this->setUnlocalizedTitle(this->m_name);

                if (this->getAttributes().empty()) {
                    ImGui::TextUnformatted("hex.builtin.nodes.custom.custom.edit_hint"_lang);
                }
            }

            this->m_editable = ImGui::GetIO().KeyShift || editing;

            ImGui::PopItemWidth();
        }

        void process() override {
            auto indexFromId = [this](u32 id) -> std::optional<u32> {
                const auto &attributes = this->getAttributes();
                for (u32 i = 0; i < attributes.size(); i++)
                    if (u32(attributes[i].getId()) == id)
                        return i;
                return std::nullopt;
            };

            auto prevContext = ImNodes::GetCurrentContext();
            ImNodes::SetCurrentContext(this->m_workspace.context.get());
            ON_SCOPE_EXIT { ImNodes::SetCurrentContext(prevContext); };

            // Forward inputs to input nodes values
            for (auto &attribute : this->getAttributes()) {
                auto index = indexFromId(attribute.getId());
                if (!index.has_value())
                    continue;

                if (auto input = this->findInput(attribute.getUnlocalizedName()); input != nullptr) {
                    switch (attribute.getType()) {
                        case dp::Attribute::Type::Integer: {
                            const auto &value = this->getIntegerOnInput(*index);
                            input->setValue(value);
                            break;
                        }
                        case dp::Attribute::Type::Float: {
                            const auto &value = this->getFloatOnInput(*index);
                            input->setValue(value);
                            break;
                        }
                        case dp::Attribute::Type::Buffer: {
                            const auto &value = this->getBufferOnInput(*index);
                            input->setValue(value);
                            break;
                        }
                    }
                }
            }

            // Process all nodes in our workspace
            for (auto &endNode : this->m_workspace.endNodes) {
                endNode->resetOutputData();

                for (auto &node : this->m_workspace.nodes)
                    node->resetProcessedInputs();

                endNode->process();
            }

            // Forward output node values to outputs
            for (auto &attribute : this->getAttributes()) {
                auto index = indexFromId(attribute.getId());
                if (!index.has_value())
                    continue;

                if (auto output = this->findOutput(attribute.getUnlocalizedName()); output != nullptr) {
                    switch (attribute.getType()) {
                        case dp::Attribute::Type::Integer: {
                            auto value = std::get<i128>(output->getValue());
                            this->setIntegerOnOutput(*index, value);
                            break;
                        }
                        case dp::Attribute::Type::Float: {
                            auto value = std::get<long double>(output->getValue());
                            this->setFloatOnOutput(*index, value);
                            break;
                        }
                        case dp::Attribute::Type::Buffer: {
                            auto value = std::get<std::vector<u8>>(output->getValue());
                            this->setBufferOnOutput(*index, value);
                            break;
                        }
                    }
                }
            }
        }

        void store(nlohmann::json &j) const override {
            j = nlohmann::json::object();

            j["nodes"] = this->m_dataProcessor->saveNodes(this->m_workspace);
        }

        void load(const nlohmann::json &j) override {
            this->m_dataProcessor->loadNodes(this->m_workspace, j.at("nodes"));

            this->m_name = LangEntry(this->getUnlocalizedTitle()).get();
            this->m_requiresAttributeUpdate = true;
        }

    private:
        std::vector<dp::Attribute> findAttributes() {
            std::vector<dp::Attribute> result;

            for (auto &node : this->m_workspace.nodes) {
                if (auto *inputNode = dynamic_cast<NodeCustomInput*>(node.get()); inputNode != nullptr)
                    result.emplace_back(dp::Attribute::IOType::In, inputNode->getType(), inputNode->getName());
                else if (auto *outputNode = dynamic_cast<NodeCustomOutput*>(node.get()); outputNode != nullptr)
                    result.emplace_back(dp::Attribute::IOType::Out, outputNode->getType(), outputNode->getName());
            }

            return result;
        }

        NodeCustomInput* findInput(const std::string &name) {
            for (auto &node : this->m_workspace.nodes) {
                if (auto *inputNode = dynamic_cast<NodeCustomInput*>(node.get()); inputNode != nullptr && inputNode->getName() == name)
                    return inputNode;
            }

            return nullptr;
        }

        NodeCustomOutput* findOutput(const std::string &name) {
            for (auto &node : this->m_workspace.nodes) {
                if (auto *outputNode = dynamic_cast<NodeCustomOutput*>(node.get()); outputNode != nullptr && outputNode->getName() == name)
                    return outputNode;
            }

            return nullptr;
        }

    private:
        std::string m_name = "hex.builtin.nodes.custom.custom.header"_lang;

        bool m_editable = false;

        bool m_requiresAttributeUpdate = false;
        ViewDataProcessor *m_dataProcessor;
        ViewDataProcessor::Workspace m_workspace;
    };

    ViewDataProcessor::ViewDataProcessor() : View("hex.builtin.view.data_processor.name") {
        ContentRegistry::DataProcessorNode::add<NodeCustom>("hex.builtin.nodes.custom", "hex.builtin.nodes.custom.custom", this);
        ContentRegistry::DataProcessorNode::add<NodeCustomInput>("hex.builtin.nodes.custom", "hex.builtin.nodes.custom.input");
        ContentRegistry::DataProcessorNode::add<NodeCustomOutput>("hex.builtin.nodes.custom", "hex.builtin.nodes.custom.output");

        ProjectFile::registerPerProviderHandler({
            .basePath = "data_processor.json",
            .required = false,
            .load = [this](prv::Provider *provider, const std::fs::path &basePath, Tar &tar) {
                std::string save = tar.readString(basePath);

                ViewDataProcessor::loadNodes(this->m_mainWorkspace.get(provider), nlohmann::json::parse(save));
                this->m_updateNodePositions = true;

                return true;
            },
            .store = [this](prv::Provider *provider, const std::fs::path &basePath, Tar &tar) {
                tar.writeString(basePath, ViewDataProcessor::saveNodes(this->m_mainWorkspace.get(provider)).dump(4));

                return true;
            }
        });

        EventManager::subscribe<EventProviderCreated>(this, [this](auto *provider) {
            this->m_mainWorkspace.get(provider) = { };
            this->m_workspaceStack.get(provider).push_back(&this->m_mainWorkspace.get(provider));
        });

        EventManager::subscribe<EventProviderChanged>(this, [this](const auto *, const auto *) {
            for (auto *workspace : *this->m_workspaceStack) {
                for (auto &node : workspace->nodes) {
                    node->setCurrentOverlay(nullptr);
                }

                workspace->dataOverlays.clear();
            }

            this->m_updateNodePositions = true;
        });

        EventManager::subscribe<EventDataChanged>(this, [this] {
            ViewDataProcessor::processNodes(*this->m_workspaceStack->back());
        });

        /* Import Nodes */
        ContentRegistry::Interface::addMenuItem({ "hex.builtin.menu.file", "hex.builtin.menu.file.import", "hex.builtin.menu.file.import.data_processor" }, 4050, Shortcut::None, [this]{
            fs::openFileBrowser(fs::DialogMode::Open, { {"hex.builtin.view.data_processor.name"_lang, "hexnode" } },
                                [&](const std::fs::path &path) {
                                    wolv::io::File file(path, wolv::io::File::Mode::Read);
                                    if (file.isValid()) {
                                        ViewDataProcessor::loadNodes(*this->m_mainWorkspace, file.readString());
                                        this->m_updateNodePositions = true;
                                    }
                                });
        }, ImHexApi::Provider::isValid);

        /* Export Nodes */
        ContentRegistry::Interface::addMenuItem({ "hex.builtin.menu.file", "hex.builtin.menu.file.export", "hex.builtin.menu.file.export.data_processor" }, 8050, Shortcut::None, [this]{
            fs::openFileBrowser(fs::DialogMode::Save, { {"hex.builtin.view.data_processor.name"_lang, "hexnode" } },
                                [&, this](const std::fs::path &path) {
                                    wolv::io::File file(path, wolv::io::File::Mode::Create);
                                    if (file.isValid())
                                        file.writeString(ViewDataProcessor::saveNodes(*this->m_mainWorkspace).dump(4));
                                });
        }, [this]{
            return !this->m_workspaceStack->empty() && !this->m_workspaceStack->back()->nodes.empty() && ImHexApi::Provider::isValid();
        });

        ContentRegistry::FileHandler::add({ ".hexnode" }, [this](const auto &path) {
            wolv::io::File file(path, wolv::io::File::Mode::Read);
            if (!file.isValid()) return false;

            ViewDataProcessor::loadNodes(*this->m_mainWorkspace, file.readString());
            this->m_updateNodePositions = true;

            return true;
        });
    }

    ViewDataProcessor::~ViewDataProcessor() {
        EventManager::unsubscribe<EventProviderCreated>(this);
        EventManager::unsubscribe<EventProviderChanged>(this);
        EventManager::unsubscribe<RequestChangeTheme>(this);
        EventManager::unsubscribe<EventFileLoaded>(this);
        EventManager::unsubscribe<EventDataChanged>(this);
    }


    void ViewDataProcessor::eraseLink(Workspace &workspace, int id) {
        auto link = std::find_if(workspace.links.begin(), workspace.links.end(), [&id](auto link) { return link.getId() == id; });

        if (link == workspace.links.end())
            return;

        for (auto &node : workspace.nodes) {
            for (auto &attribute : node->getAttributes()) {
                attribute.removeConnectedAttribute(id);
            }
        }

        workspace.links.erase(link);

        ImHexApi::Provider::markDirty();
    }

    void ViewDataProcessor::eraseNodes(Workspace &workspace, const std::vector<int> &ids) {
        for (int id : ids) {
            auto node = std::find_if(workspace.nodes.begin(), workspace.nodes.end(),
            [&id](const auto &node) {
                return node->getId() == id;
            });

            for (auto &attr : (*node)->getAttributes()) {
                std::vector<int> linksToRemove;
                for (auto &[linkId, connectedAttr] : attr.getConnectedAttributes())
                    linksToRemove.push_back(linkId);

                for (auto linkId : linksToRemove)
                    eraseLink(workspace, linkId);
            }
        }

        for (int id : ids) {
            auto node = std::find_if(workspace.nodes.begin(), workspace.nodes.end(), [&id](const auto &node) { return node->getId() == id; });

            std::erase_if(workspace.endNodes, [&id](const auto &node) { return node->getId() == id; });

            workspace.nodes.erase(node);
        }

        ImHexApi::Provider::markDirty();
    }

    void ViewDataProcessor::processNodes(Workspace &workspace) {
        if (workspace.dataOverlays.size() != workspace.endNodes.size()) {
            for (auto overlay : workspace.dataOverlays)
                ImHexApi::Provider::get()->deleteOverlay(overlay);
            workspace.dataOverlays.clear();

            for (u32 i = 0; i < workspace.endNodes.size(); i++)
                workspace.dataOverlays.push_back(ImHexApi::Provider::get()->newOverlay());
        }

        u32 overlayIndex = 0;
        for (auto endNode : workspace.endNodes) {
            endNode->setCurrentOverlay(workspace.dataOverlays[overlayIndex]);
            overlayIndex++;
        }

        workspace.currNodeError.reset();

        try {
            for (auto &endNode : workspace.endNodes) {
                endNode->resetOutputData();

                for (auto &node : workspace.nodes)
                    node->resetProcessedInputs();

                endNode->process();
            }
        } catch (dp::Node::NodeError &e) {
            workspace.currNodeError = e;

            for (auto overlay : workspace.dataOverlays)
                ImHexApi::Provider::get()->deleteOverlay(overlay);
            workspace.dataOverlays.clear();

        } catch (std::runtime_error &e) {
            log::fatal("Node implementation bug! {}", e.what());
        }
    }

    void ViewDataProcessor::reloadCustomNodes() {
        this->m_customNodes.clear();

        for (const auto &basePath : fs::getDefaultPaths(fs::ImHexPath::Nodes)) {
            for (const auto &entry : std::fs::recursive_directory_iterator(basePath)) {
                if (entry.path().extension() == ".hexnode") {
                    try {
                        nlohmann::json nodeJson = nlohmann::json::parse(wolv::io::File(entry.path(), wolv::io::File::Mode::Read).readString());

                        this->m_customNodes.push_back(CustomNode { LangEntry(nodeJson.at("name")), nodeJson });
                    } catch (nlohmann::json::exception &e) {
                        continue;
                    }
                }
            }
        }
    }

    void ViewDataProcessor::drawContent() {
        auto &workspace = *this->m_workspaceStack->back();

        bool popWorkspace = false;
        if (ImGui::Begin(View::toWindowName("hex.builtin.view.data_processor.name").c_str(), &this->getWindowOpenState(), ImGuiWindowFlags_NoCollapse)) {
            ImNodes::SetCurrentContext(workspace.context.get());

            if (ImGui::IsMouseReleased(ImGuiMouseButton_Right) && ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows)) {
                ImNodes::ClearNodeSelection();
                ImNodes::ClearLinkSelection();

                this->m_rightClickedCoords = ImGui::GetMousePos();

                if (ImNodes::IsNodeHovered(&this->m_rightClickedId))
                    ImGui::OpenPopup("Node Menu");
                else if (ImNodes::IsLinkHovered(&this->m_rightClickedId))
                    ImGui::OpenPopup("Link Menu");
                else {
                    ImGui::OpenPopup("Context Menu");
                    this->reloadCustomNodes();
                }
            }

            if (ImGui::BeginPopup("Context Menu")) {
                std::unique_ptr<dp::Node> node;

                if (ImNodes::NumSelectedNodes() > 0 || ImNodes::NumSelectedLinks() > 0) {
                    if (ImGui::MenuItem("hex.builtin.view.data_processor.name"_lang)) {
                        std::vector<int> ids;
                        ids.resize(ImNodes::NumSelectedNodes());
                        ImNodes::GetSelectedNodes(ids.data());

                        this->eraseNodes(workspace, ids);
                        ImNodes::ClearNodeSelection();

                        ids.resize(ImNodes::NumSelectedLinks());
                        ImNodes::GetSelectedLinks(ids.data());

                        for (auto id : ids)
                            this->eraseLink(workspace, id);
                        ImNodes::ClearLinkSelection();
                    }
                }

                for (const auto &[unlocalizedCategory, unlocalizedName, function] : ContentRegistry::DataProcessorNode::impl::getEntries()) {
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

                if (ImGui::BeginMenu("hex.builtin.nodes.custom"_lang)) {
                    ImGui::Separator();

                    for (auto &customNode : this->m_customNodes) {
                        if (ImGui::MenuItem(customNode.name.c_str())) {
                            node = loadNode(customNode.data);
                        }
                    }
                    ImGui::EndMenu();
                }

                if (node != nullptr) {
                    bool hasOutput = false;
                    bool hasInput  = false;
                    for (auto &attr : node->getAttributes()) {
                        if (attr.getIOType() == dp::Attribute::IOType::Out)
                            hasOutput = true;

                        if (attr.getIOType() == dp::Attribute::IOType::In)
                            hasInput = true;
                    }

                    if (hasInput && !hasOutput)
                        workspace.endNodes.push_back(node.get());

                    ImNodes::SetNodeScreenSpacePos(node->getId(), this->m_rightClickedCoords);
                    workspace.nodes.push_back(std::move(node));
                    ImHexApi::Provider::markDirty();
                }

                ImGui::EndPopup();
            }

            if (ImGui::BeginPopup("Node Menu")) {
                if (ImGui::MenuItem("hex.builtin.view.data_processor.menu.save_node"_lang)) {
                    auto it = std::find_if(workspace.nodes.begin(), workspace.nodes.end(),
                        [this](const auto &node) {
                            return node->getId() == this->m_rightClickedId;
                        });

                    if (it != workspace.nodes.end()) {
                        auto &node = *it;
                        fs::openFileBrowser(fs::DialogMode::Save, { {"hex.builtin.view.data_processor.name"_lang, "hexnode" } }, [&](const std::fs::path &path){
                            wolv::io::File outputFile(path, wolv::io::File::Mode::Create);
                            outputFile.writeString(ViewDataProcessor::saveNode(node.get()).dump(4));
                        });
                    }
                }

                ImGui::Separator();

                if (ImGui::MenuItem("hex.builtin.view.data_processor.menu.remove_node"_lang))
                    this->eraseNodes(workspace, { this->m_rightClickedId });

                ImGui::EndPopup();
            }

            if (ImGui::BeginPopup("Link Menu")) {
                if (ImGui::MenuItem("hex.builtin.view.data_processor.menu.remove_link"_lang))
                    this->eraseLink(workspace, this->m_rightClickedId);

                ImGui::EndPopup();
            }

            {
                int nodeId;
                if (ImNodes::IsNodeHovered(&nodeId) && workspace.currNodeError.has_value() && workspace.currNodeError->node->getId() == nodeId) {
                    ImGui::BeginTooltip();
                    ImGui::TextUnformatted("hex.builtin.common.error"_lang);
                    ImGui::Separator();
                    ImGui::TextUnformatted(workspace.currNodeError->message.c_str());
                    ImGui::EndTooltip();
                }
            }

            if (ImGui::BeginChild("##node_editor", ImGui::GetContentRegionAvail() - ImVec2(0, ImGui::GetTextLineHeightWithSpacing() * 1.3F))) {
                ImNodes::BeginNodeEditor();

                for (auto &node : workspace.nodes) {
                    ImNodes::SnapNodeToGrid(node->getId());

                    const bool hasError = workspace.currNodeError.has_value() && workspace.currNodeError->node == node.get();

                    if (hasError)
                        ImNodes::PushColorStyle(ImNodesCol_NodeOutline, 0xFF0000FF);

                    int nodeId = node->getId();
                    if (this->m_updateNodePositions) {
                        this->m_updateNodePositions = false;
                        ImNodes::SetNodeGridSpacePos(nodeId, node->getPosition());
                    } else {
                        if (ImNodes::ObjectPoolFind(ImNodes::EditorContextGet().Nodes, nodeId) >= 0)
                            node->setPosition(ImNodes::GetNodeGridSpacePos(nodeId));
                    }

                    ImNodes::BeginNode(nodeId);

                    ImNodes::BeginNodeTitleBar();
                    ImGui::TextUnformatted(LangEntry(node->getUnlocalizedTitle()));
                    ImNodes::EndNodeTitleBar();

                    ImGui::PopStyleVar();
                    node->drawNode();
                    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(1.0F, 1.0F));

                    for (auto &attribute : node->getAttributes()) {
                        ImNodesPinShape pinShape;

                        auto attributeType = attribute.getType();
                        switch (attributeType) {
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

                            if (attribute.getConnectedAttributes().empty() && attributeType != dp::Attribute::Type::Buffer) {
                                auto &defaultValue = attribute.getDefaultData();

                                ImGui::PushItemWidth(100_scaled);
                                if (attributeType == dp::Attribute::Type::Integer) {
                                    defaultValue.resize(sizeof(i128));

                                    auto value = i64(*reinterpret_cast<i128*>(defaultValue.data()));
                                    if (ImGui::InputScalar(LangEntry(attribute.getUnlocalizedName()), ImGuiDataType_S64, &value)) {
                                        std::fill(defaultValue.begin(), defaultValue.end(), 0x00);

                                        i128 writeValue = value;
                                        std::memcpy(defaultValue.data(), &writeValue, sizeof(writeValue));
                                    }
                                } else if (attributeType == dp::Attribute::Type::Float) {
                                    defaultValue.resize(sizeof(long double));

                                    auto value = double(*reinterpret_cast<long double*>(defaultValue.data()));
                                    if (ImGui::InputScalar(LangEntry(attribute.getUnlocalizedName()), ImGuiDataType_Double, &value)) {
                                        std::fill(defaultValue.begin(), defaultValue.end(), 0x00);

                                        long double writeValue = value;
                                        std::memcpy(defaultValue.data(), &writeValue, sizeof(writeValue));
                                    }
                                }
                                ImGui::PopItemWidth();

                            } else {
                                ImGui::TextUnformatted(LangEntry(attribute.getUnlocalizedName()));
                            }

                            ImNodes::EndInputAttribute();
                        } else if (attribute.getIOType() == dp::Attribute::IOType::Out) {
                            ImNodes::BeginOutputAttribute(attribute.getId(), ImNodesPinShape(pinShape + 1));
                            ImGui::TextUnformatted(LangEntry(attribute.getUnlocalizedName()));
                            ImNodes::EndOutputAttribute();
                        }
                    }

                    ImNodes::EndNode();

                    ImNodes::SetNodeGridSpacePos(nodeId, node->getPosition());

                    if (hasError)
                        ImNodes::PopColorStyle();
                }

                std::vector<int> linksToRemove;
                for (const auto &link : workspace.links) {
                    if (ImNodes::ObjectPoolFind(ImNodes::EditorContextGet().Pins, link.getFromId()) == -1 ||
                        ImNodes::ObjectPoolFind(ImNodes::EditorContextGet().Pins, link.getToId()) == -1) {

                        linksToRemove.push_back(link.getId());
                    }
                }
                for (auto linkId : linksToRemove)
                    this->eraseLink(workspace, linkId);

                for (const auto &link : workspace.links) {
                    ImNodes::Link(link.getId(), link.getFromId(), link.getToId());
                }

                ImNodes::MiniMap(0.2F, ImNodesMiniMapLocation_BottomRight);

                if (workspace.nodes.empty())
                    ImGui::TextFormattedCentered("{}", "hex.builtin.view.data_processor.help_text"_lang);

                if (this->m_workspaceStack->size() > 1) {
                    ImGui::SetCursorPos(ImVec2(ImGui::GetContentRegionAvail().x - ImGui::GetTextLineHeightWithSpacing() * 1.5F, ImGui::GetTextLineHeightWithSpacing() * 0.2F));
                    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4.0F, 4.0F));
                    if (ImGui::DimmedIconButton(ICON_VS_CLOSE, ImGui::GetCustomColorVec4(ImGuiCustomCol_ToolbarRed))) {
                        popWorkspace = true;
                    }
                    ImGui::PopStyleVar();
                }

                ImNodes::EndNodeEditor();
            }
            ImGui::EndChild();

            if (ImGui::IconButton(ICON_VS_DEBUG_START, ImGui::GetCustomColorVec4(ImGuiCustomCol_ToolbarGreen)) || this->m_continuousEvaluation)
                this->processNodes(workspace);

            ImGui::SameLine();

            ImGui::Checkbox("Continuous evaluation", &this->m_continuousEvaluation);


            {
                int linkId;
                if (ImNodes::IsLinkDestroyed(&linkId)) {
                    this->eraseLink(workspace, linkId);
                }
            }

            {
                int from, to;
                if (ImNodes::IsLinkCreated(&from, &to)) {

                    do {
                        dp::Attribute *fromAttr = nullptr, *toAttr = nullptr;
                        for (auto &node : workspace.nodes) {
                            for (auto &attribute : node->getAttributes()) {
                                if (attribute.getId() == from)
                                    fromAttr = &attribute;
                                else if (attribute.getId() == to)
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

                        auto newLink = workspace.links.emplace_back(from, to);

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
                        eraseLink(workspace, id);
                    }
                }
            }

            {
                const int selectedNodeCount = ImNodes::NumSelectedNodes();
                if (selectedNodeCount > 0 && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Delete))) {
                    static std::vector<int> selectedNodes;
                    selectedNodes.resize(static_cast<size_t>(selectedNodeCount));
                    ImNodes::GetSelectedNodes(selectedNodes.data());

                    this->eraseNodes(workspace, selectedNodes);
                }
            }
        }
        ImGui::End();

        if (popWorkspace) {
            this->m_workspaceStack->pop_back();
            this->m_updateNodePositions = true;
        }
    }

    nlohmann::json ViewDataProcessor::saveNode(const dp::Node *node) {
        nlohmann::json output;

        output["name"] = node->getUnlocalizedTitle();
        output["type"] = node->getUnlocalizedName();

        nlohmann::json nodeData;
        node->store(nodeData);
        output["data"] = nodeData;

        output["attrs"] = nlohmann::json::array();
        u32 attrIndex = 0;
        for (auto &attr : node->getAttributes()) {
            output["attrs"][attrIndex] = attr.getId();
            attrIndex++;
        }

        return output;
    }

    nlohmann::json ViewDataProcessor::saveNodes(const ViewDataProcessor::Workspace &workspace) {
        nlohmann::json output;

        output["nodes"] = nlohmann::json::object();
        for (auto &node : workspace.nodes) {
            auto id         = node->getId();
            auto &currNodeOutput = output["nodes"][std::to_string(id)];
            auto pos     = node->getPosition();

            currNodeOutput = saveNode(node.get());
            currNodeOutput["id"] = id;
            currNodeOutput["pos"]  = {
                    { "x", pos.x },
                    { "y", pos.y }
            };
        }

        output["links"] = nlohmann::json::object();
        for (auto &link : workspace.links) {
            auto id          = link.getId();
            auto &currOutput = output["links"][std::to_string(id)];

            currOutput["id"]   = id;
            currOutput["from"] = link.getFromId();
            currOutput["to"]   = link.getToId();
        }

        return output;
    }

    std::unique_ptr<dp::Node> ViewDataProcessor::loadNode(const nlohmann::json &node) {
        try {

            auto &nodeEntries = ContentRegistry::DataProcessorNode::impl::getEntries();

            std::unique_ptr<dp::Node> newNode;
            for (auto &entry : nodeEntries) {
                if (node.contains("name") && entry.name == node["type"].get<std::string>())
                    newNode = entry.creatorFunction();
            }

            if (newNode == nullptr)
                return nullptr;

            if (node.contains("id"))
                newNode->setId(node["id"]);

            if (node.contains("name"))
                newNode->setUnlocalizedTitle(node["name"]);

            u32 attrIndex  = 0;
            for (auto &attr : newNode->getAttributes()) {
                attr.setId(node["attrs"][attrIndex]);
                attrIndex++;
            }

            if (!node["data"].is_null())
                newNode->load(node["data"]);

            return newNode;
        } catch (nlohmann::json::exception &e) {
            return nullptr;
        }
    }

    void ViewDataProcessor::loadNodes(ViewDataProcessor::Workspace &workspace, const nlohmann::json &jsonData) {
        workspace.nodes.clear();
        workspace.endNodes.clear();
        workspace.links.clear();

        try {
            for (auto &node : jsonData["nodes"]) {
                auto newNode = loadNode(node);
                if (newNode == nullptr)
                    continue;

                newNode->setPosition(ImVec2(node["pos"]["x"], node["pos"]["y"]));

                if (!node["data"].is_null())
                    newNode->load(node["data"]);

                bool hasOutput = false;
                bool hasInput  = false;
                u32 attrIndex  = 0;
                for (auto &attr : newNode->getAttributes()) {
                    if (attr.getIOType() == dp::Attribute::IOType::Out)
                        hasOutput = true;

                    if (attr.getIOType() == dp::Attribute::IOType::In)
                        hasInput = true;

                    attr.setId(node["attrs"][attrIndex]);
                    attrIndex++;
                }

                if (hasInput && !hasOutput)
                    workspace.endNodes.push_back(newNode.get());

                workspace.nodes.push_back(std::move(newNode));
            }

            int maxLinkId = 0;
            for (auto &link : jsonData["links"]) {
                dp::Link newLink(link["from"], link["to"]);

                int linkId = link["id"];
                maxLinkId  = std::max(linkId, maxLinkId);

                newLink.setId(linkId);
                workspace.links.push_back(newLink);

                dp::Attribute *fromAttr = nullptr, *toAttr = nullptr;
                for (auto &node : workspace.nodes) {
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

            int maxNodeId = 0;
            int maxAttrId = 0;
            for (auto &node : workspace.nodes) {
                maxNodeId = std::max(maxNodeId, node->getId());

                for (auto &attr : node->getAttributes()) {
                    maxAttrId = std::max(maxAttrId, attr.getId());
                }
            }

            dp::Node::setIdCounter(maxNodeId + 1);
            dp::Attribute::setIdCounter(maxAttrId + 1);
            dp::Link::setIdCounter(maxLinkId + 1);

            ViewDataProcessor::processNodes(workspace);
        } catch (nlohmann::json::exception &e) {
            PopupError::open(hex::format("Failed to load nodes: {}", e.what()));
        }
    }

}