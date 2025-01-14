#include "content/views/view_data_processor.hpp"
#include <toasts/toast_notification.hpp>

#include <hex/api/content_registry.hpp>
#include <hex/api/project_file_manager.hpp>
#include <hex/api/achievement_manager.hpp>

#include <hex/providers/provider.hpp>
#include <hex/helpers/logger.hpp>
#include <hex/helpers/default_paths.hpp>

#include <imnodes.h>
#include <imnodes_internal.h>
#include <nlohmann/json.hpp>

#include <wolv/io/file.hpp>
#include <wolv/utils/guards.hpp>
#include <wolv/utils/core.hpp>

namespace hex::plugin::builtin {

    /**
     * @brief Input node that can be used to add an input to a custom node
     */
    class NodeCustomInput : public dp::Node {
    public:
        NodeCustomInput() : Node("hex.builtin.nodes.custom.input.header", { dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Integer, "hex.builtin.nodes.common.input") }) { }
        ~NodeCustomInput() override = default;

        void drawNode() override {
            ImGui::PushItemWidth(100_scaled);
            // Draw combo box to select the type of the input
            if (ImGui::Combo("##type", &m_type, "Integer\0Float\0Buffer\0")) {
                this->setAttributes({
                    { dp::Attribute(dp::Attribute::IOType::Out, this->getType(), "hex.builtin.nodes.common.input") }
                });
            }

            // Draw text input to set the name of the input
            if (ImGui::InputText("##name", m_name)) {
                this->setUnlocalizedTitle(m_name);
            }

            ImGui::PopItemWidth();
        }

        void setValue(auto value) { m_value = std::move(value); }

        const std::string &getName() const { return m_name; }

        dp::Attribute::Type getType() const {
            switch (m_type) {
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
            }, m_value);
        }

        void store(nlohmann::json &j) const override {
            j = nlohmann::json::object();

            j["name"] = m_name;
            j["type"] = m_type;
        }

        void load(const nlohmann::json &j) override {
            m_name = j.at("name").get<std::string>();
            m_type = j.at("type");

            this->setUnlocalizedTitle(m_name);
            this->setAttributes({
                { dp::Attribute(dp::Attribute::IOType::Out, this->getType(), "hex.builtin.nodes.common.input") }
            });
        }

    private:
        std::string m_name = Lang(this->getUnlocalizedName());
        int m_type = 0;

        std::variant<i128, long double, std::vector<u8>> m_value;
    };

    /**
     * @brief Input node that can be used to add an output to a custom node
     */
    class NodeCustomOutput : public dp::Node {
    public:
        NodeCustomOutput() : Node("hex.builtin.nodes.custom.output.header", { dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Integer, "hex.builtin.nodes.common.output") }) { }
        ~NodeCustomOutput() override = default;

        void drawNode() override {
            ImGui::PushItemWidth(100_scaled);

            // Draw combo box to select the type of the output
            if (ImGui::Combo("##type", &m_type, "Integer\0Float\0Buffer\0")) {
                this->setAttributes({
                    { dp::Attribute(dp::Attribute::IOType::In, this->getType(), "hex.builtin.nodes.common.output") }
                });
            }

            // Draw text input to set the name of the output
            if (ImGui::InputText("##name", m_name)) {
                this->setUnlocalizedTitle(m_name);
            }

            ImGui::PopItemWidth();
        }

        const std::string &getName() const { return m_name; }
        dp::Attribute::Type getType() const {
            switch (m_type) {
                case 0: return dp::Attribute::Type::Integer;
                case 1: return dp::Attribute::Type::Float;
                case 2: return dp::Attribute::Type::Buffer;
                default: return dp::Attribute::Type::Integer;
            }
        }

        void process() override {
            switch (this->getType()) {
                case dp::Attribute::Type::Integer: m_value = this->getIntegerOnInput(0); break;
                case dp::Attribute::Type::Float:   m_value = static_cast<long double>(this->getFloatOnInput(0)); break;
                case dp::Attribute::Type::Buffer:  m_value = this->getBufferOnInput(0); break;
            }
        }

        const auto& getValue() const { return m_value; }

        void store(nlohmann::json &j) const override {
            j = nlohmann::json::object();

            j["name"] = m_name;
            j["type"] = m_type;
        }

        void load(const nlohmann::json &j) override {
            m_name = j.at("name").get<std::string>();
            m_type = j.at("type");

            this->setUnlocalizedTitle(m_name);
            this->setAttributes({
                { dp::Attribute(dp::Attribute::IOType::In, this->getType(), "hex.builtin.nodes.common.output") }
            });
        }

    private:
        std::string m_name = Lang(this->getUnlocalizedName());
        int m_type = 0;

        std::variant<i128, long double, std::vector<u8>> m_value;
    };

    /**
     * @brief Custom node that can be used to create custom data processing nodes
     */
    class NodeCustom : public dp::Node {
    public:
        explicit NodeCustom(ViewDataProcessor *dataProcessor) : Node("hex.builtin.nodes.custom.custom.header", {}), m_dataProcessor(dataProcessor) { }
        ~NodeCustom() override = default;

        void drawNode() override {
            // Update attributes if we have to
            if (m_requiresAttributeUpdate) {
                m_requiresAttributeUpdate = false;

                // Find all input and output nodes that are used by the workspace of this node
                // and set the attributes of this node to the attributes of the input and output nodes
                this->setAttributes(this->findAttributes());
            }

            ImGui::PushItemWidth(200_scaled);

            bool editing = false;
            if (m_editable) {
                // Draw name input field
                ImGuiExt::InputTextIcon("##name", ICON_VS_SYMBOL_KEY, m_name);

                // Prevent editing mode from deactivating when the input field is focused
                editing = ImGui::IsItemActive();

                // Draw edit button
                if (ImGui::Button("hex.builtin.nodes.custom.custom.edit"_lang, ImVec2(200_scaled, ImGui::GetTextLineHeightWithSpacing()))) {
                    AchievementManager::unlockAchievement("hex.builtin.achievement.data_processor", "hex.builtin.achievement.data_processor.custom_node.name");

                    // Open the custom node's workspace
                    m_dataProcessor->getWorkspaceStack().push_back(&m_workspace);

                    m_requiresAttributeUpdate = true;
                    m_dataProcessor->updateNodePositions();
                }
            } else {
                this->setUnlocalizedTitle(m_name);

                if (this->getAttributes().empty()) {
                    ImGui::TextUnformatted("hex.builtin.nodes.custom.custom.edit_hint"_lang);
                }
            }

            // Enable editing mode when the shift button is pressed
            m_editable = ImGui::GetIO().KeyShift || editing;

            ImGui::PopItemWidth();
        }

        void process() override {
            // Find the index of an attribute by its id
            auto indexFromId = [this](u32 id) -> std::optional<u32> {
                const auto &attributes = this->getAttributes();
                for (u32 i = 0; i < attributes.size(); i++) {
                    if (u32(attributes[i].getId()) == id)
                        return i;
                }
                return std::nullopt;
            };

            auto prevContext = ImNodes::GetCurrentContext();
            ImNodes::SetCurrentContext(m_workspace.context.get());
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
            for (auto &endNode : m_workspace.endNodes) {
                endNode->resetOutputData();

                // Reset processed inputs of all nodes
                for (auto &node : m_workspace.nodes)
                    node->resetProcessedInputs();

                endNode->process();
            }

            // Forward output node values to outputs
            for (auto &attribute : this->getAttributes()) {
                // Find the index of the attribute
                auto index = indexFromId(attribute.getId());
                if (!index.has_value())
                    continue;

                // Find the node that is connected to the attribute
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

            j["nodes"] = m_dataProcessor->saveNodes(m_workspace);
        }

        void load(const nlohmann::json &j) override {
            m_dataProcessor->loadNodes(m_workspace, j.at("nodes"));

            m_name = Lang(this->getUnlocalizedTitle()).get();
            m_requiresAttributeUpdate = true;
        }

    private:
        std::vector<dp::Attribute> findAttributes() const {
            std::vector<dp::Attribute> result;

            // Search through all nodes in the workspace and add all input and output nodes to the result
            for (auto &node : m_workspace.nodes) {
                if (auto *inputNode = dynamic_cast<NodeCustomInput*>(node.get()); inputNode != nullptr)
                    result.emplace_back(dp::Attribute::IOType::In, inputNode->getType(), inputNode->getName());
                else if (auto *outputNode = dynamic_cast<NodeCustomOutput*>(node.get()); outputNode != nullptr)
                    result.emplace_back(dp::Attribute::IOType::Out, outputNode->getType(), outputNode->getName());
            }

            return result;
        }

        NodeCustomInput* findInput(const std::string &name) const {
            for (auto &node : m_workspace.nodes) {
                if (auto *inputNode = dynamic_cast<NodeCustomInput*>(node.get()); inputNode != nullptr && inputNode->getName() == name)
                    return inputNode;
            }

            return nullptr;
        }

        NodeCustomOutput* findOutput(const std::string &name) const {
            for (auto &node : m_workspace.nodes) {
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

    ViewDataProcessor::ViewDataProcessor() : View::Window("hex.builtin.view.data_processor.name", ICON_VS_CHIP) {
        ContentRegistry::DataProcessorNode::add<NodeCustom>("hex.builtin.nodes.custom", "hex.builtin.nodes.custom.custom", this);
        ContentRegistry::DataProcessorNode::add<NodeCustomInput>("hex.builtin.nodes.custom", "hex.builtin.nodes.custom.input");
        ContentRegistry::DataProcessorNode::add<NodeCustomOutput>("hex.builtin.nodes.custom", "hex.builtin.nodes.custom.output");

        ProjectFile::registerPerProviderHandler({
            .basePath = "data_processor.json",
            .required = false,
            .load = [this](prv::Provider *provider, const std::fs::path &basePath, const Tar &tar) {
                std::string save = tar.readString(basePath);

                ViewDataProcessor::loadNodes(m_mainWorkspace.get(provider), nlohmann::json::parse(save));
                m_updateNodePositions = true;

                return true;
            },
            .store = [this](prv::Provider *provider, const std::fs::path &basePath, const Tar &tar) {
                tar.writeString(basePath, ViewDataProcessor::saveNodes(m_mainWorkspace.get(provider)).dump(4));

                return true;
            }
        });

        EventProviderCreated::subscribe(this, [this](auto *provider) {
            m_mainWorkspace.get(provider) = { };
            m_workspaceStack.get(provider).push_back(&m_mainWorkspace.get(provider));
        });

        EventProviderChanged::subscribe(this, [this](const auto *, const auto *) {
            for (auto *workspace : *m_workspaceStack) {
                for (auto &node : workspace->nodes) {
                    node->setCurrentOverlay(nullptr);
                }

                workspace->dataOverlays.clear();
            }

            m_updateNodePositions = true;
        });

        /* Import Nodes */
        ContentRegistry::Interface::addMenuItem({ "hex.builtin.menu.file", "hex.builtin.menu.file.import", "hex.builtin.menu.file.import.data_processor" }, ICON_VS_CHIP, 4050, Shortcut::None, [this]{
            fs::openFileBrowser(fs::DialogMode::Open, { {"hex.builtin.view.data_processor.name"_lang, "hexnode" } },
                                [&](const std::fs::path &path) {
                                    wolv::io::File file(path, wolv::io::File::Mode::Read);
                                    if (file.isValid()) {
                                        ViewDataProcessor::loadNodes(*m_mainWorkspace, nlohmann::json::parse(file.readString()));
                                        m_updateNodePositions = true;
                                    }
                                });
        }, ImHexApi::Provider::isValid);

        /* Export Nodes */
        ContentRegistry::Interface::addMenuItem({ "hex.builtin.menu.file", "hex.builtin.menu.file.export", "hex.builtin.menu.file.export.data_processor" }, ICON_VS_CHIP, 8050, Shortcut::None, [this]{
            fs::openFileBrowser(fs::DialogMode::Save, { {"hex.builtin.view.data_processor.name"_lang, "hexnode" } },
                                [&, this](const std::fs::path &path) {
                                    wolv::io::File file(path, wolv::io::File::Mode::Create);
                                    if (file.isValid())
                                        file.writeString(ViewDataProcessor::saveNodes(*m_mainWorkspace).dump(4));
                                });
        }, [this]{
            return !m_workspaceStack->empty() && !m_workspaceStack->back()->nodes.empty() && ImHexApi::Provider::isValid();
        });

        ContentRegistry::FileHandler::add({ ".hexnode" }, [this](const auto &path) {
            wolv::io::File file(path, wolv::io::File::Mode::Read);
            if (!file.isValid()) return false;

            ViewDataProcessor::loadNodes(*m_mainWorkspace, file.readString());
            m_updateNodePositions = true;

            return true;
        });
    }

    ViewDataProcessor::~ViewDataProcessor() {
        EventProviderCreated::unsubscribe(this);
        EventProviderChanged::unsubscribe(this);
        RequestChangeTheme::unsubscribe(this);
        EventFileLoaded::unsubscribe(this);
        EventDataChanged::unsubscribe(this);
    }


    void ViewDataProcessor::eraseLink(Workspace &workspace, int id) {
        // Find the link with the given ID
        auto link = std::find_if(workspace.links.begin(), workspace.links.end(),
                                 [&id](auto link) {
                                     return link.getId() == id;
                                 });

        // Return if the link was not found
        if (link == workspace.links.end())
            return;

        // Remove the link from all attributes that are connected to it
        for (auto &node : workspace.nodes) {
            for (auto &attribute : node->getAttributes()) {
                attribute.removeConnectedAttribute(id);
            }
        }

        // Remove the link from the workspace
        workspace.links.erase(link);

        ImHexApi::Provider::markDirty();
    }

    void ViewDataProcessor::eraseNodes(Workspace &workspace, const std::vector<int> &ids) {
        // Loop over the IDs of all nodes that should be removed
        // and remove all links that are connected to the attributes of the node
        for (int id : ids) {
            // Find the node with the given ID
            auto node = std::find_if(workspace.nodes.begin(), workspace.nodes.end(),
                                    [&id](const auto &node) {
                                        return node->getId() == id;
                                    });

            // Loop over all attributes of that node
            for (auto &attr : (*node)->getAttributes()) {
                std::vector<int> linksToRemove;

                // Find all links that are connected to the attribute and remove them
                for (auto &[linkId, connectedAttr] : attr.getConnectedAttributes())
                    linksToRemove.push_back(linkId);

                // Remove the links from the workspace
                for (auto linkId : linksToRemove)
                    eraseLink(workspace, linkId);
            }
        }

        // Loop over the IDs of all nodes that should be removed
        // and remove the nodes from the workspace
        for (int id : ids) {
            // Find the node with the given ID
            auto node = std::find_if(workspace.nodes.begin(), workspace.nodes.end(),
                                     [&id](const auto &node) {
                                         return node->getId() == id;
                                     });

            // Remove the node from the workspace
            std::erase_if(workspace.endNodes,
                          [&id](const auto &node) {
                            return node->getId() == id;
                          });

            if (workspace.currNodeError.has_value()) {
                if (workspace.currNodeError->node == node->get()) {
                    workspace.currNodeError.reset();
                }
            }

            // Remove the node from the workspace
            workspace.nodes.erase(node);
        }

        ImHexApi::Provider::markDirty();
    }

    void ViewDataProcessor::processNodes(Workspace &workspace) {
        // If the number of end nodes does not match the number of data overlays,
        // the overlays have to be recreated
        if (workspace.dataOverlays.size() != workspace.endNodes.size()) {
            // Delete all the overlays from the current provider
            for (auto overlay : workspace.dataOverlays)
                ImHexApi::Provider::get()->deleteOverlay(overlay);
            workspace.dataOverlays.clear();

            // Create a new overlay for each end node
            for (u32 i = 0; i < workspace.endNodes.size(); i += 1)
                workspace.dataOverlays.push_back(ImHexApi::Provider::get()->newOverlay());
        }

        // Set the current overlay of each end node to the corresponding overlay
        size_t overlayIndex = 0;
        for (auto endNode : workspace.endNodes) {
            endNode->setCurrentOverlay(workspace.dataOverlays[overlayIndex]);
            overlayIndex += 1;
        }

        // Reset any potential node errors
        workspace.currNodeError.reset();

        m_evaluationTask = TaskManager::createTask("hex.builtin.task.evaluating_nodes"_lang, 0, [this, workspace = &workspace](Task& task) {
            task.setInterruptCallback([]{
                dp::Node::interrupt();
            });

            const auto handleError = [workspace] {
                TaskManager::doLater([workspace] {
                    // Delete all overlays
                    for (auto overlay : workspace->dataOverlays)
                        ImHexApi::Provider::get()->deleteOverlay(overlay);
                    workspace->dataOverlays.clear();
                });
            };

            do {
                // Process all nodes in the workspace
                try {
                    for (auto &endNode : workspace->endNodes) {
                        task.update();

                        // Reset the output data of the end node
                        endNode->resetOutputData();

                        // Reset processed inputs of all nodes
                        for (auto &node : workspace->nodes) {
                            node->reset();
                            node->resetProcessedInputs();
                        }

                        // Process the end node
                        endNode->process();
                    }
                } catch (const dp::Node::NodeError &e) {
                    // Handle user errors

                    // Add the node error to the current workspace, so it can be displayed
                    workspace->currNodeError = e;
                    handleError();

                    break;
                } catch (const std::runtime_error &e) {
                    // Handle internal errors

                    log::fatal("Data processor node implementation bug! {}", e.what());
                    handleError();

                    break;
                } catch (const std::exception &e) {
                    // Handle other fatal errors

                    log::fatal("Unhandled exception thrown in data processor node! {}", e.what());
                    handleError();

                    break;
                }
            } while (m_continuousEvaluation);
        });

    }

    void ViewDataProcessor::reloadCustomNodes() {
        // Delete all custom nodes
        m_customNodes.clear();

        // Loop over all custom node folders
        for (const auto &basePath : paths::Nodes.read()) {
            // Loop over all files in the folder
            for (const auto &entry : std::fs::recursive_directory_iterator(basePath)) {
                // Skip files that are not .hexnode files
                if (entry.path().extension() != ".hexnode")
                    continue;

                try {
                    // Load the content of the file as JSON
                    wolv::io::File file(entry.path(), wolv::io::File::Mode::Read);
                    nlohmann::json nodeJson = nlohmann::json::parse(file.readString());

                    // Add the loaded node to the list of custom nodes
                    m_customNodes.push_back(CustomNode { Lang(nodeJson.at("name").get<std::string>()), nodeJson });
                } catch (nlohmann::json::exception &e) {
                    log::warn("Failed to load custom node '{}': {}", entry.path().string(), e.what());
                }
            }
        }
    }

    void ViewDataProcessor::updateNodePositions() {
        m_updateNodePositions = true;
    }


    void ViewDataProcessor::drawContextMenus(ViewDataProcessor::Workspace &workspace) {
        // Handle the right click context menus
        if (ImGui::IsMouseDown(ImGuiMouseButton_Right) && ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows) && !ImGui::IsMouseDragging(ImGuiMouseButton_Right)) {
            // Clear selections
            ImNodes::ClearNodeSelection();
            ImNodes::ClearLinkSelection();

            // Save the current mouse position
            m_rightClickedCoords = ImGui::GetMousePos();

            // Show a different context menu depending on if a node, a link
            // or the background was right-clicked
            if (ImNodes::IsNodeHovered(&m_rightClickedId)) {
                ImGui::OpenPopup("Node Menu");
            } else if (ImNodes::IsLinkHovered(&m_rightClickedId)) {
                ImGui::OpenPopup("Link Menu");
            } else {
                ImGui::OpenPopup("Context Menu");
                this->reloadCustomNodes();
            }
        }

        // Draw main context menu
        if (ImGui::BeginPopup("Context Menu")) {
            std::unique_ptr<dp::Node> node;

            // Check if any link or node has been selected previously
            if (ImNodes::NumSelectedNodes() > 0 || ImNodes::NumSelectedLinks() > 0) {
                if (ImGui::MenuItem("hex.builtin.view.data_processor.menu.remove_selection"_lang)) {
                    // Delete selected nodes
                    {
                        std::vector<int> ids;
                        ids.resize(ImNodes::NumSelectedNodes());
                        ImNodes::GetSelectedNodes(ids.data());

                        this->eraseNodes(workspace, ids);
                        ImNodes::ClearNodeSelection();
                    }

                    // Delete selected links
                    {
                        std::vector<int> ids;
                        ids.resize(ImNodes::NumSelectedLinks());
                        ImNodes::GetSelectedLinks(ids.data());

                        for (auto id : ids)
                            this->eraseLink(workspace, id);
                        ImNodes::ClearLinkSelection();
                    }
                }
            }

            // Draw all nodes that are registered in the content registry
            for (const auto &[unlocalizedCategory, unlocalizedName, function] : ContentRegistry::DataProcessorNode::impl::getEntries()) {
                if (unlocalizedCategory.empty() && unlocalizedName.empty()) {
                    // Draw a separator if the node has no category and no name
                    ImGui::Separator();
                } else if (unlocalizedCategory.empty()) {
                    // Draw the node if it has no category
                    if (ImGui::MenuItem(Lang(unlocalizedName))) {
                        node = function();
                    }
                } else {
                    // Draw the node inside its sub menu if it has a category
                    if (ImGui::BeginMenu(Lang(unlocalizedCategory))) {
                        if (ImGui::MenuItem(Lang(unlocalizedName))) {
                            node = function();
                        }
                        ImGui::EndMenu();
                    }
                }
            }

            // Draw custom nodes submenu
            if (ImGui::BeginMenu("hex.builtin.nodes.custom"_lang)) {
                ImGui::Separator();

                // Draw entries for each custom node
                for (auto &customNode : m_customNodes) {
                    if (ImGui::MenuItem(customNode.name.c_str())) {
                        node = loadNode(customNode.data);
                    }
                }
                ImGui::EndMenu();
            }

            // Check if a node has been selected in the menu
            if (node != nullptr) {
                // Place the node in the workspace

                // Check if the node has inputs and/or outputs
                bool hasOutput = false;
                bool hasInput  = false;
                for (auto &attr : node->getAttributes()) {
                    if (attr.getIOType() == dp::Attribute::IOType::Out)
                        hasOutput = true;

                    if (attr.getIOType() == dp::Attribute::IOType::In)
                        hasInput = true;
                }

                // If the node has only inputs and no outputs, it's considered an end node
                // Add it to the list of end nodes to be processed
                if (hasInput && !hasOutput)
                    workspace.endNodes.push_back(node.get());

                // Set the position of the node to the position where the user right-clicked
                ImNodes::SetNodeScreenSpacePos(node->getId(), m_rightClickedCoords);
                node->setPosition(m_rightClickedCoords);

                workspace.nodes.push_back(std::move(node));

                ImHexApi::Provider::markDirty();
                AchievementManager::unlockAchievement("hex.builtin.achievement.data_processor", "hex.builtin.achievement.data_processor.place_node.name");
            }

            ImGui::EndPopup();
        }

        // Draw node right click menu
        if (ImGui::BeginPopup("Node Menu")) {
            if (ImGui::MenuItem("hex.builtin.view.data_processor.menu.save_node"_lang)) {
                // Find the node that was right-clicked
                auto it = std::find_if(workspace.nodes.begin(), workspace.nodes.end(),
                                       [this](const auto &node) {
                                           return node->getId() == m_rightClickedId;
                                       });

                // Check if the node was found
                if (it != workspace.nodes.end()) {
                    auto &node = *it;

                    // Save the node to a file
                    fs::openFileBrowser(fs::DialogMode::Save, { {"hex.builtin.view.data_processor.name"_lang, "hexnode" } }, [&](const std::fs::path &path){
                        wolv::io::File outputFile(path, wolv::io::File::Mode::Create);
                        outputFile.writeString(ViewDataProcessor::saveNode(node.get()).dump(4));
                    });
                }
            }

            ImGui::Separator();

            if (ImGui::MenuItem("hex.builtin.view.data_processor.menu.remove_node"_lang))
                this->eraseNodes(workspace, { m_rightClickedId });

            ImGui::EndPopup();
        }

        // Draw link right click menu
        if (ImGui::BeginPopup("Link Menu")) {
            if (ImGui::MenuItem("hex.builtin.view.data_processor.menu.remove_link"_lang))
                this->eraseLink(workspace, m_rightClickedId);

            ImGui::EndPopup();
        }
    }

    void ViewDataProcessor::drawNode(dp::Node &node) const {
        // If a node position update is pending, update the node position
        int nodeId = node.getId();
        if (m_updateNodePositions) {
            ImNodes::SetNodeGridSpacePos(nodeId, node.getPosition());
        } else {
            if (ImNodes::ObjectPoolFind(ImNodes::EditorContextGet().Nodes, nodeId) >= 0)
                node.setPosition(ImNodes::GetNodeGridSpacePos(nodeId));
        }

        // Draw the node
        ImNodes::BeginNode(nodeId);
        {
            // Draw the node's title bar
            ImNodes::BeginNodeTitleBar();
            {
                ImGui::TextUnformatted(Lang(node.getUnlocalizedTitle()));
            }
            ImNodes::EndNodeTitleBar();

            // Draw the node's body
            ImGui::PopStyleVar();
            if (!m_evaluationTask.isRunning()) {
                node.draw();
            }
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(1.0F, 1.0F));

            // Draw all attributes of the node
            for (auto &attribute : node.getAttributes()) {
                ImNodesPinShape pinShape;

                // Set the pin shape depending on the attribute type
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

                // Draw the attribute
                if (attribute.getIOType() == dp::Attribute::IOType::In) {
                    ImNodes::BeginInputAttribute(attribute.getId(), pinShape);

                    // Draw input fields for attributes that have no connected links
                    if (attribute.getConnectedAttributes().empty() && attributeType != dp::Attribute::Type::Buffer) {
                        auto &defaultValue = attribute.getDefaultData();

                        ImGui::PushItemWidth(100_scaled);
                        if (attributeType == dp::Attribute::Type::Integer) {
                            defaultValue.resize(sizeof(i128));

                            auto value = i64(*reinterpret_cast<i128*>(defaultValue.data()));
                            if (ImGui::InputScalar(Lang(attribute.getUnlocalizedName()), ImGuiDataType_S64, &value)) {
                                std::fill(defaultValue.begin(), defaultValue.end(), 0x00);

                                i128 writeValue = value;
                                std::memcpy(defaultValue.data(), &writeValue, sizeof(writeValue));
                            }
                        } else if (attributeType == dp::Attribute::Type::Float) {
                            defaultValue.resize(sizeof(long double));

                            auto value = double(*reinterpret_cast<long double*>(defaultValue.data()));
                            if (ImGui::InputScalar(Lang(attribute.getUnlocalizedName()), ImGuiDataType_Double, &value)) {
                                std::fill(defaultValue.begin(), defaultValue.end(), 0x00);

                                long double writeValue = value;
                                std::memcpy(defaultValue.data(), &writeValue, sizeof(writeValue));
                            }
                        }
                        ImGui::PopItemWidth();

                    } else {
                        ImGui::TextUnformatted(Lang(attribute.getUnlocalizedName()));
                    }

                    ImNodes::EndInputAttribute();
                } else if (attribute.getIOType() == dp::Attribute::IOType::Out) {
                    ImNodes::BeginOutputAttribute(attribute.getId(), ImNodesPinShape(pinShape + 1));
                    ImGui::TextUnformatted(Lang(attribute.getUnlocalizedName()));
                    ImNodes::EndOutputAttribute();
                }
            }
        }



        ImNodes::EndNode();

        ImNodes::SetNodeGridSpacePos(nodeId, node.getPosition());
    }

    void ViewDataProcessor::drawContent() {
        auto &workspace = *m_workspaceStack->back();

        ImGui::BeginDisabled(m_evaluationTask.isRunning());

        bool popWorkspace = false;
        // Set the ImNodes context to the current workspace context
        auto prevContext = ImNodes::GetCurrentContext();
        ImNodes::SetCurrentContext(workspace.context.get());
        ON_SCOPE_EXIT { ImNodes::SetCurrentContext(prevContext); };

        this->drawContextMenus(workspace);

        // Draw error tooltip when hovering over a node that has an error
        {
            int nodeId;
            if (ImNodes::IsNodeHovered(&nodeId) && workspace.currNodeError.has_value() && workspace.currNodeError->node->getId() == nodeId) {
                ImGui::BeginTooltip();
                ImGui::TextUnformatted("hex.ui.common.error"_lang);
                ImGui::Separator();
                ImGui::TextUnformatted(workspace.currNodeError->message.c_str());
                ImGui::EndTooltip();
            }
        }

        // Draw the main node editor workspace window
        if (ImGui::BeginChild("##node_editor", ImGui::GetContentRegionAvail() - ImVec2(0, ImGui::GetTextLineHeightWithSpacing() * 1.3F))) {
            ImNodes::BeginNodeEditor();

            if (m_evaluationTask.isRunning())
                ImNodes::GetCurrentContext()->MousePos = { FLT_MAX, FLT_MAX };

            // Loop over all nodes that have been placed in the workspace
            bool stillUpdating = m_updateNodePositions;
            for (auto &node : workspace.nodes) {
                ImNodes::SnapNodeToGrid(node->getId());

                // If the node has an error, draw it with a red outline
                const bool hasError = workspace.currNodeError.has_value() && workspace.currNodeError->node == node.get();
                if (hasError)
                    ImNodes::PushColorStyle(ImNodesCol_NodeOutline, 0xFF0000FF);

                // Draw the node
                this->drawNode(*node);

                if (hasError)
                    ImNodes::PopColorStyle();
            }

            if (stillUpdating)
                m_updateNodePositions = false;

            // Handle removing links that are connected to attributes that don't exist anymore
            {
                std::vector<int> linksToRemove;
                for (const auto &link : workspace.links) {
                    if (ImNodes::ObjectPoolFind(ImNodes::EditorContextGet().Pins, link.getFromId()) == -1 ||
                        ImNodes::ObjectPoolFind(ImNodes::EditorContextGet().Pins, link.getToId()) == -1) {

                        linksToRemove.push_back(link.getId());
                    }
                }
                for (auto linkId : linksToRemove)
                    this->eraseLink(workspace, linkId);
            }

            // Draw links
            for (const auto &link : workspace.links) {
                ImNodes::Link(link.getId(), link.getFromId(), link.getToId());
            }

            // Draw the mini map in the bottom right
            ImNodes::MiniMap(0.2F, ImNodesMiniMapLocation_BottomRight);

            // Draw the help text if no nodes have been placed yet
            if (workspace.nodes.empty())
                ImGuiExt::TextFormattedCentered("{}", "hex.builtin.view.data_processor.help_text"_lang);

            // Draw a close button if there is more than one workspace on the stack
            if (m_workspaceStack->size() > 1) {
                ImGui::SetCursorPos(ImVec2(ImGui::GetContentRegionAvail().x - ImGui::GetTextLineHeightWithSpacing() * 1.5F, ImGui::GetTextLineHeightWithSpacing() * 0.2F));
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4.0F, 4.0F));
                if (ImGuiExt::DimmedIconButton(ICON_VS_CLOSE, ImGuiExt::GetCustomColorVec4(ImGuiCustomCol_ToolbarRed))) {
                    popWorkspace = true;
                }
                ImGui::PopStyleVar();
            }

            ImNodes::EndNodeEditor();
        }
        ImGui::EndChild();

        ImGui::EndDisabled();

        // Draw the control bar at the bottom
        {
            if (!m_evaluationTask.isRunning()) {
                if (ImGuiExt::IconButton(ICON_VS_DEBUG_START, ImGuiExt::GetCustomColorVec4(ImGuiCustomCol_ToolbarGreen))) {
                    this->processNodes(workspace);
                }
            } else {
                if (ImGuiExt::IconButton(ICON_VS_DEBUG_STOP, ImGuiExt::GetCustomColorVec4(ImGuiCustomCol_ToolbarRed))) {
                    m_evaluationTask.interrupt();
                }
            }

            ImGui::SameLine();

            ImGui::Checkbox("Continuous evaluation", &m_continuousEvaluation);
        }


        // Erase links that have been destroyed
        {
            int linkId;
            if (ImNodes::IsLinkDestroyed(&linkId)) {
                this->eraseLink(workspace, linkId);
            }
        }

        // Handle creation of new links
        {
            int from, to;
            if (ImNodes::IsLinkCreated(&from, &to)) {

                do {
                    dp::Attribute *fromAttr = nullptr, *toAttr = nullptr;

                    // Find the attributes that are connected by the link
                    for (auto &node : workspace.nodes) {
                        for (auto &attribute : node->getAttributes()) {
                            if (attribute.getId() == from)
                                fromAttr = &attribute;
                            else if (attribute.getId() == to)
                                toAttr = &attribute;
                        }
                    }

                    // If one of the attributes could not be found, the link is invalid and can't be created
                    if (fromAttr == nullptr || toAttr == nullptr)
                        break;

                    // If the attributes have different types, don't create the link
                    if (fromAttr->getType() != toAttr->getType())
                        break;

                    // If the link tries to connect two input or two output attributes, don't create the link
                    if (fromAttr->getIOType() == toAttr->getIOType())
                        break;

                    // If the link tries to connect to a input attribute that already has a link connected to it, don't create the link
                    if (!toAttr->getConnectedAttributes().empty())
                        break;

                    // Add a new link to the current workspace
                    auto newLink = workspace.links.emplace_back(from, to);

                    // Add the link to the attributes that are connected by it
                    fromAttr->addConnectedAttribute(newLink.getId(), toAttr);
                    toAttr->addConnectedAttribute(newLink.getId(), fromAttr);

                    AchievementManager::unlockAchievement("hex.builtin.achievement.data_processor", "hex.builtin.achievement.data_processor.create_connection.name");
                } while (false);
            }
        }

        // Handle deletion of links using the Delete key
        {
            const int selectedLinkCount = ImNodes::NumSelectedLinks();
            if (selectedLinkCount > 0 && ImGui::IsKeyPressed(ImGuiKey_Delete)) {
                std::vector<int> selectedLinks;
                selectedLinks.resize(static_cast<size_t>(selectedLinkCount));
                ImNodes::GetSelectedLinks(selectedLinks.data());
                ImNodes::ClearLinkSelection();

                for (const int id : selectedLinks) {
                    eraseLink(workspace, id);
                }
            }
        }

        // Handle deletion of noes using the Delete key
        {
            const int selectedNodeCount = ImNodes::NumSelectedNodes();
            if (selectedNodeCount > 0 && ImGui::IsKeyPressed(ImGuiKey_Delete)) {
                std::vector<int> selectedNodes;
                selectedNodes.resize(static_cast<size_t>(selectedNodeCount));
                ImNodes::GetSelectedNodes(selectedNodes.data());
                ImNodes::ClearNodeSelection();

                this->eraseNodes(workspace, selectedNodes);
            }
        }

        // Remove the top-most workspace from the stack if requested
        if (popWorkspace) {
            m_workspaceStack->pop_back();
            m_updateNodePositions = true;
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

    std::unique_ptr<dp::Node> ViewDataProcessor::loadNode(nlohmann::json data) {
        try {
            auto &nodeEntries = ContentRegistry::DataProcessorNode::impl::getEntries();

            std::unique_ptr<dp::Node> newNode;
            for (auto &entry : nodeEntries) {
                if (data.contains("name") && entry.unlocalizedName == data["type"].get<std::string>())
                    newNode = entry.creatorFunction();
            }

            if (newNode == nullptr)
                return nullptr;

            if (data.contains("id"))
                newNode->setId(data["id"]);

            if (data.contains("name"))
                newNode->setUnlocalizedTitle(data["name"]);

            u32 attrIndex  = 0;
            for (auto &attr : newNode->getAttributes()) {
                if (attrIndex < data["attrs"].size())
                    attr.setId(data["attrs"][attrIndex]);
                else
                    attr.setId(-1);

                attrIndex++;
            }

            if (!data["data"].is_null())
                newNode->load(data["data"]);

            return newNode;
        } catch (nlohmann::json::exception &e) {
            log::error("Failed to load node: {}", e.what());
            return nullptr;
        }
    }

    void ViewDataProcessor::loadNodes(ViewDataProcessor::Workspace &workspace, nlohmann::json data) {
        workspace.nodes.clear();
        workspace.endNodes.clear();
        workspace.links.clear();

        try {
            for (auto &node : data["nodes"]) {
                auto newNode = loadNode(node);
                if (newNode == nullptr)
                    continue;

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

                    if (attrIndex < node["attrs"].size())
                        attr.setId(node["attrs"][attrIndex]);
                    else
                        attr.setId(-1);

                    attrIndex++;
                }

                if (hasInput && !hasOutput)
                    workspace.endNodes.push_back(newNode.get());

                newNode->setPosition(ImVec2(node["pos"]["x"], node["pos"]["y"]));

                workspace.nodes.push_back(std::move(newNode));
            }

            int maxLinkId = 0;
            for (auto &link : data["links"]) {
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
                    continue;

                if (fromAttr->getType() != toAttr->getType())
                    continue;

                if (fromAttr->getIOType() == toAttr->getIOType())
                    continue;

                if (!toAttr->getConnectedAttributes().empty())
                    continue;

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

            for (auto &node : workspace.nodes) {
                if (node->getId() == -1) {
                    maxNodeId += 1;
                    node->setId(maxNodeId);
                }
            }

            dp::Node::setIdCounter(maxNodeId + 1);
            dp::Attribute::setIdCounter(maxAttrId + 1);
            dp::Link::setIdCounter(maxLinkId + 1);

            m_updateNodePositions = true;
        } catch (nlohmann::json::exception &e) {
            ui::ToastError::open(hex::format("Failed to load nodes: {}", e.what()));
        }
    }

}