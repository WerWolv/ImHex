#include <content/views/view_disassembler.hpp>
#include <hex/api/content_registry/user_interface.hpp>
#include <hex/api/content_registry/views.hpp>
#include <hex/api/events/events_interaction.hpp>
#include <hex/api/imhex_api/hex_editor.hpp>

#include <hex/providers/provider.hpp>
#include <hex/helpers/fmt.hpp>

#include <fonts/vscode_icons.hpp>
#include <imgui_internal.h>

#include <toasts/toast_notification.hpp>

#include <wolv/literals.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
#include <tuple>
#include <unordered_map>

using namespace std::literals::string_literals;
using namespace wolv::literals;

namespace hex::plugin::disasm {

    ViewDisassembler::ViewDisassembler() : View::Window("hex.disassembler.view.disassembler.name", ICON_VS_FILE_CODE) {
        EventProviderDeleted::subscribe(this, [this](const auto *provider) {
            m_disassembly.get(provider).clear();
            m_flowEdges.get(provider).clear();
            m_returnPrefix.get(provider).clear();
            m_selectedInstruction.get(provider).reset();
            m_selectedRegion.get(provider) = Region::Invalid();
            m_scrollToSelectedInstruction.get(provider) = false;
        });

        EventRegionSelected::subscribe(this, [this](const auto &selection) {
            this->updateSelection(selection.getProvider(), selection.getRegion());
        });

        ContentRegistry::UserInterface::addMenuItem({ "hex.builtin.menu.edit", "hex.builtin.menu.edit.disassemble_range" }, ICON_VS_DEBUG_LINE_BY_LINE, 3100, CTRLCMD + SHIFT + Keys::D, [this] {
            ImGui::SetWindowFocus(this->getName().c_str());
            this->getWindowOpenState() = true;

            m_range = ui::RegionType::Region;
            m_regionToDisassemble = ImHexApi::HexEditor::getSelection()->getRegion();

            this->disassemble();
        }, [this]{
            return ImHexApi::HexEditor::isSelectionValid() && !m_disassemblerTask.isRunning() && *m_currArchitecture != nullptr;
        }, ContentRegistry::Views::getViewByName("hex.builtin.view.hex_editor.name"));

        m_settingsCollapsed.setOnCreateCallback([](auto *, bool &value) { value = false; });
    }

    ViewDisassembler::~ViewDisassembler() {
        EventProviderDeleted::unsubscribe(this);
        EventRegionSelected::unsubscribe(this);
    }

    void ViewDisassembler::updateSelection(prv::Provider *provider, const Region &region) {
        if (provider == nullptr) {
            provider = ImHexApi::Provider::get();
            if (provider == nullptr)
                return;
        }

        m_selectedRegion.get(provider) = region;
        if (m_disassemblerTask.isRunning())
            return;

        std::optional<std::size_t> selectedInstruction;
        const auto &disassembly = m_disassembly.get(provider);
        const u64 imageBaseAddress = m_imageBaseAddress.get(provider);
        if (region != Region::Invalid() && region.getStartAddress() >= imageBaseAddress) {
            const u64 offset = region.getStartAddress() - imageBaseAddress;

            auto instruction = std::upper_bound(disassembly.begin(), disassembly.end(), offset, [](u64 value, const auto &candidate) {
                return value < candidate.offset;
            });

            if (instruction != disassembly.begin()) {
                instruction -= 1;
                if (offset >= instruction->offset && offset - instruction->offset < instruction->size)
                    selectedInstruction = std::distance(disassembly.begin(), instruction);
            }
        }

        auto &previousSelection = m_selectedInstruction.get(provider);
        m_scrollToSelectedInstruction.get(provider) = selectedInstruction.has_value() && selectedInstruction != previousSelection;
        previousSelection = selectedInstruction;
    }

    std::vector<ViewDisassembler::FlowEdge> ViewDisassembler::findFlowEdges(const std::vector<ContentRegistry::Disassemblers::Instruction> &disassembly) {
        std::unordered_map<u64, std::size_t> instructionIndices;
        instructionIndices.reserve(disassembly.size());
        for (std::size_t i = 0; i < disassembly.size(); i += 1)
            instructionIndices.emplace(disassembly[i].address, i);

        std::vector<FlowEdge> edges;
        for (std::size_t source = 0; source < disassembly.size(); source += 1) {
            const auto &instruction = disassembly[source];
            if ((instruction.type != ContentRegistry::Disassemblers::InstructionType::Jump &&
                 instruction.type != ContentRegistry::Disassemblers::InstructionType::Call) || !instruction.targetAddress.has_value())
                continue;

            const auto target = instructionIndices.find(*instruction.targetAddress);
            if (target != instructionIndices.end() && target->second != source) {
                edges.push_back({
                    .source = source,
                    .target = target->second,
                    .lane = 0,
                    .sourceSlot = 0,
                    .sourceSlotCount = 0,
                    .targetSlot = 0,
                    .targetSlotCount = 0
                });
            }
        }

        std::ranges::sort(edges, [](const auto &left, const auto &right) {
            return std::tuple(std::min(left.source, left.target), std::max(left.source, left.target), left.source, left.target) <
                   std::tuple(std::min(right.source, right.target), std::max(right.source, right.target), right.source, right.target);
        });

        struct DestinationGroup {
            std::size_t target;
            std::size_t start;
            std::size_t end;
            std::size_t lane;
        };

        std::vector<DestinationGroup> groups;
        std::unordered_map<std::size_t, std::size_t> groupIndices;
        for (const auto &edge : edges) {
            const std::size_t start = std::min(edge.source, edge.target);
            const std::size_t end = std::max(edge.source, edge.target);
            const auto [entry, inserted] = groupIndices.emplace(edge.target, groups.size());
            if (inserted) {
                groups.push_back({ edge.target, start, end, 0 });
            } else {
                auto &group = groups[entry->second];
                group.start = std::min(group.start, start);
                group.end = std::max(group.end, end);
            }
        }

        std::ranges::sort(groups, [](const auto &left, const auto &right) {
            return std::tuple(left.start, left.end, left.target) < std::tuple(right.start, right.end, right.target);
        });

        std::vector<std::size_t> laneEnds;
        std::vector<std::size_t> destinationLanes(disassembly.size(), 0);
        for (auto &group : groups) {
            const std::size_t start = group.start;

            auto lane = std::ranges::find_if(
                laneEnds,
                [start](std::size_t laneEnd) {
                    return laneEnd < start;
                }
            );

            if (lane == laneEnds.end()) {
                group.lane = laneEnds.size();
                laneEnds.push_back(group.end);
            } else {
                group.lane = std::distance(laneEnds.begin(), lane);
                *lane = group.end;
            }

            destinationLanes[group.target] = group.lane;
        }

        for (auto &edge : edges)
            edge.lane = destinationLanes[edge.target];

        std::vector<std::size_t> endpointCounts(disassembly.size(), 0);
        std::vector<bool> hasIncomingEdge(disassembly.size(), false);
        for (const auto &edge : edges) {
            endpointCounts[edge.source] += 1;
            hasIncomingEdge[edge.target] = true;
        }
        for (std::size_t i = 0; i < hasIncomingEdge.size(); i += 1)
            endpointCounts[i] += hasIncomingEdge[i] ? 1 : 0;

        std::vector<std::size_t> nextEndpointSlot(disassembly.size(), 0);
        std::vector<std::size_t> destinationSlots(disassembly.size(), std::numeric_limits<std::size_t>::max());
        for (auto &edge : edges) {
            edge.sourceSlot = nextEndpointSlot[edge.source];
            nextEndpointSlot[edge.source] += 1;
            edge.sourceSlotCount = endpointCounts[edge.source];

            if (destinationSlots[edge.target] == std::numeric_limits<std::size_t>::max()) {
                destinationSlots[edge.target] = nextEndpointSlot[edge.target];
                nextEndpointSlot[edge.target] += 1;
            }
            edge.targetSlot = destinationSlots[edge.target];
            edge.targetSlotCount = endpointCounts[edge.target];
        }

        return edges;
    }

    void ViewDisassembler::disassemble() {
        const auto provider = ImHexApi::Provider::get();

        m_disassembly.get(provider).clear();
        m_flowEdges.get(provider).clear();
        m_returnPrefix.get(provider).clear();
        m_selectedInstruction.get(provider).reset();
        m_scrollToSelectedInstruction.get(provider) = false;

        if (m_regionToDisassemble.get(provider).getStartAddress() < m_imageBaseAddress)
            return;

        m_disassemblerTask = TaskManager::createTask("hex.disassembler.view.disassembler.disassembling", m_regionToDisassemble.get(provider).getSize(), [this, provider](auto &task) {
            const auto &currArchitecture = m_currArchitecture.get(provider);
            const auto region = m_regionToDisassemble.get(provider);
            auto &disassembly = m_disassembly.get(provider);

            if (currArchitecture == nullptr)
                return;

            // Create a disassembler instance
            if (currArchitecture->start()) {
                ON_SCOPE_EXIT {
                    currArchitecture->end();

                    if (!disassembly.empty()) {
                        TaskManager::doLater([this, provider]{ m_settingsCollapsed.get(provider) = true; });
                    }
                };

                std::vector<u8> buffer(1_MiB, 0x00);

                const u64 codeOffset = region.getStartAddress() - m_imageBaseAddress;

                // Read the data in chunks and disassemble it
                u64 instructionLoadAddress = m_imageLoadAddress + codeOffset;
                u64 instructionDataAddress = region.getStartAddress();

                bool hadError = false;
                while (instructionDataAddress <= region.getEndAddress()) {
                    // Read a chunk of data
                    std::size_t bufferSize = std::min<u64>(buffer.size(), (region.getEndAddress()-instructionDataAddress)+1);
                    provider->read(instructionDataAddress, buffer.data(), bufferSize);

                    auto code = std::span(buffer.data(), bufferSize);

                    // Ask the backend to disassemble the data
                    while (true) {
                        auto instruction = currArchitecture->disassemble(m_imageBaseAddress, instructionLoadAddress, instructionDataAddress, code);
                        if (!instruction.has_value())
                            break;

                        task.update(instructionDataAddress);

                        disassembly.push_back(instruction.value());

                        if (instruction->size == 0 || instruction->size > code.size())
                            break;

                        code = code.subspan(instruction->size);
                        instructionDataAddress += instruction->size;
                        instructionLoadAddress += instruction->size;
                        hadError = false;

                        if (code.empty())
                            break;
                    }

                    if (hadError) break;
                    hadError = true;
                }

                m_flowEdges.get(provider) = findFlowEdges(disassembly);
                auto &returnPrefix = m_returnPrefix.get(provider);
                returnPrefix.resize(disassembly.size() + 1, 0);
                for (std::size_t i = 0; i < disassembly.size(); i += 1) {
                    returnPrefix[i + 1] = returnPrefix[i];

                    if (disassembly[i].type == ContentRegistry::Disassemblers::InstructionType::Return)
                        returnPrefix[i + 1] += 1;
                }

                TaskManager::doLater([this, provider] {
                    this->updateSelection(provider, m_selectedRegion.get(provider));
                });
            }
        });
    }

    void ViewDisassembler::exportToFile() {
        const auto provider = ImHexApi::Provider::get();
        TaskManager::createTask("hex.ui.common.processing", TaskManager::NoProgress, [this, provider](auto &) {
            TaskManager::doLater([this, provider] {
                fs::openFileBrowser(fs::DialogMode::Save, {}, [this, provider](const std::fs::path &path) {
                    auto p = path;
                    if (p.extension() != ".asm")
                        p.replace_filename(fmt::format("{}{}", p.filename().string(), ".asm"));
                    auto file = wolv::io::File(p, wolv::io::File::Mode::Create);

                    if (!file.isValid()) {
                        ui::ToastError::open("hex.disassembler.view.disassembler.export.popup.error"_lang);
                        return;
                    }

                    // As disassembly code can be quite long, we prefer writing each disassembled instruction to file
                    for (const ContentRegistry::Disassemblers::Instruction& instruction : m_disassembly.get(provider)) {
                        // We test for a "bugged" case that should never happen - the instruction should always have a mnemonic
                        if (instruction.mnemonic.empty())
                            continue;

                        if (instruction.operators.empty())
                            file.writeString(fmt::format("{}\n", instruction.mnemonic));
                        else
                            file.writeString(fmt::format("{} {}\n", instruction.mnemonic, instruction.operators));
                    }
                });
            });
        });
    }

    void ViewDisassembler::drawContent() {
        auto *provider = ImHexApi::Provider::get();

        if (ImHexApi::Provider::isValid() && provider->isReadable()) {
            auto &region = m_regionToDisassemble.get(provider);
            auto &range = m_range.get(provider);

            auto &collapsed = m_settingsCollapsed.get(provider);
            ImGui::SetNextWindowScroll(ImVec2(0, 0));
            if (ImGuiExt::BeginSubWindow("hex.ui.common.settings"_lang, &collapsed, collapsed ? ImVec2(0, 1) : ImVec2(0, 0))) {
                ImGui::BeginDisabled(m_disassemblerTask.isRunning());
                {
                    // Draw region selection picker
                    ui::regionSelectionPicker(&region, provider, &range, false, true);

                    ImGui::SameLine();
                    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
                    ImGui::SameLine();

                    // Draw base address input
                    ImGui::BeginGroup();
                    {
                        auto &address = m_imageLoadAddress.get(provider);
                        ImGuiExt::InputHexadecimal("hex.disassembler.view.disassembler.image_load_address"_lang, &address, ImGuiInputTextFlags_CharsHexadecimal);
                        ImGui::SameLine();
                        ImGuiExt::HelpHover("hex.disassembler.view.disassembler.image_load_address.hint"_lang, ICON_VS_INFO);
                    }

                    // Draw code region start address input
                    ImGui::BeginDisabled(m_range == ui::RegionType::EntireData);
                    {
                        auto &address = m_imageBaseAddress.get(provider);
                        ImGuiExt::InputHexadecimal("hex.disassembler.view.disassembler.image_base_address"_lang, &address, ImGuiInputTextFlags_CharsHexadecimal);
                        ImGui::SameLine();
                        ImGuiExt::HelpHover("hex.disassembler.view.disassembler.image_base_address.hint"_lang, ICON_VS_INFO);
                    }
                    ImGui::EndDisabled();
                    ImGui::EndGroup();

                    // Draw settings
                    {
                        ImGui::Separator();

                        // Draw architecture selector
                        const auto &architectures = ContentRegistry::Disassemblers::impl::getArchitectures();
                        if (architectures.empty()) {
                            ImGuiExt::TextSpinner("hex.disassembler.view.disassembler.arch"_lang);
                        } else {
                            const auto &currArchitecture = m_currArchitecture.get(provider);
                            if (currArchitecture == nullptr) {
                                m_currArchitecture = architectures.begin()->second();
                            }

                            if (ImGui::BeginTabBar("Architecture", ImGuiTabBarFlags_TabListPopupButton | ImGuiTabBarFlags_FittingPolicyScroll | ImGuiTabBarFlags_DrawSelectedOverline)) {
                                for (const auto &[name, creator] : architectures) {
                                    if (ImGui::BeginTabItem(name.c_str())) {
                                        if (m_currArchitecture->get()->getName() != name) {
                                            m_currArchitecture = creator();
                                        }

                                        ImGui::EndTabItem();
                                    }
                                }
                                ImGui::EndTabBar();
                            }

                            // Draw sub-settings for each architecture
                            if (currArchitecture->hasSettings()) {
                                if (ImGuiExt::BeginBox()) {
                                    currArchitecture->drawSettings();
                                }
                                ImGuiExt::EndBox();
                            }
                        }
                    }
                }
                ImGui::EndDisabled();
            }
            ImGuiExt::EndSubWindow();

            // Draw disassemble button
            ImGui::BeginDisabled(m_disassemblerTask.isRunning() || region.getStartAddress() < m_imageBaseAddress);
            {
                if (ImGuiExt::DimmedButton("hex.disassembler.view.disassembler.disassemble"_lang))
                    this->disassemble();
            }
            ImGui::EndDisabled();

            const auto &disassembly = m_disassembly.get(provider);

            // Draw export to file icon button
            ImGui::SameLine();
            ImGui::BeginDisabled(m_disassemblerTask.isRunning() || disassembly.empty());
            {
                if (ImGuiExt::DimmedIconButton(ICON_VS_EXPORT, ImGui::GetStyleColorVec4(ImGuiCol_Text)))
                    this->exportToFile();
            }
            ImGui::EndDisabled();
            ImGuiExt::InfoTooltip("hex.disassembler.view.disassembler.export"_lang);

            // Draw a spinner if the disassembler is running
            if (m_disassemblerTask.isRunning()) {
                ImGui::SameLine();
                ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
                ImGui::SameLine();

                ImGuiExt::TextSpinner("hex.disassembler.view.disassembler.disassembling"_lang);
            }

            ImGui::NewLine();

            // Draw code listing with control-flow gutter.
            constexpr static std::size_t MaxVisibleFlowLanes = 12;

            const auto &flowEdges = m_flowEdges.get(provider);
            const auto &returnPrefix = m_returnPrefix.get(provider);

            const bool isDisassembling = m_disassemblerTask.isRunning();

            const std::size_t laneCount = isDisassembling || flowEdges.empty()
                ? 0
                : std::min(MaxVisibleFlowLanes, std::ranges::max_element(flowEdges, { }, &FlowEdge::lane)->lane + 1);

            const float rowHeight = ImGui::GetTextLineHeight() + ImGui::GetStyle().CellPadding.y * 2;
            const float returnGap = ImGui::GetFontSize() * 0.65F;
            const float laneSpacing = ImGui::GetFontSize() * 0.55F;

            const float gutterWidth = std::max(ImGui::GetFontSize() * 1.5F, laneCount * laneSpacing + ImGui::GetFontSize());
            const float addressWidth = ImGui::CalcTextSize("0x0000000000000000").x + ImGui::GetStyle().ItemSpacing.x * 2;
            const float bytesWidth = ImGui::CalcTextSize("00 00 00 00 00 ...").x + ImGui::GetStyle().ItemSpacing.x * 2;
            const float typeIconWidth = rowHeight + ImGui::GetStyle().ItemSpacing.x;

            const bool showOffsets = ImGui::GetIO().KeyShift;
            const auto &selectedInstruction = m_selectedInstruction.get(provider);
            auto &scrollToSelection = m_scrollToSelectedInstruction.get(provider);

            auto *drawList = ImGui::GetWindowDrawList();

            if (ImGui::BeginChild("##disassemblyListing", ImVec2(0, 0), ImGuiChildFlags_Borders)) {
                if (!isDisassembling) {
                    drawList = ImGui::GetWindowDrawList();
                    const ImVec2 rowsOrigin = ImGui::GetCursorScreenPos();
                    const ImVec2 childMin = ImGui::GetWindowPos();
                    const ImVec2 childMax = childMin + ImGui::GetWindowSize();
                    const float contentStartY = ImGui::GetCursorPosY();

                    const float childWidth = ImGui::GetContentRegionAvail().x;
                    const float rowAddressX = rowsOrigin.x + gutterWidth;
                    const float rowBytesX = rowAddressX + addressWidth;
                    const float rowTypeIconX = rowBytesX + bytesWidth;
                    const float rowInstructionX = rowTypeIconX + typeIconWidth;

                    const ImVec4 bytesClip(rowBytesX, childMin.y, rowTypeIconX - ImGui::GetStyle().ItemSpacing.x, childMax.y);
                    const ImVec4 instructionClip(rowInstructionX, childMin.y, childMax.x, childMax.y);

                    const auto returnCountBefore = [&returnPrefix](size_t index) {
                        return index < returnPrefix.size() ? returnPrefix[index] : size_t(0);
                    };

                    const auto rowTop = [rowHeight, returnGap, &returnCountBefore](size_t index) {
                        return float(index) * rowHeight + float(returnCountBefore(index)) * returnGap;
                    };

                    const float contentHeight = rowTop(disassembly.size());

                    if (scrollToSelection && selectedInstruction.has_value()) {
                        ImGui::SetScrollY(std::max(0.0F, contentStartY + rowTop(*selectedInstruction) - ImGui::GetWindowHeight() * 0.5F));
                        scrollToSelection = false;
                    }

                    const auto rowAtY = [&rowTop, &disassembly](float y) {
                        std::size_t first = 0;
                        std::size_t last = disassembly.size();
                        while (first < last) {
                            const std::size_t middle = first + (last - first) / 2;
                            if (rowTop(middle) < y)
                                first = middle + 1;
                            else
                                last = middle;
                        }
                        return first;
                    };

                    const float visibleTop = childMin.y - rowsOrigin.y;
                    const float visibleBottom = childMax.y - rowsOrigin.y;
                    std::size_t firstVisible = rowAtY(visibleTop);
                    if (firstVisible > 0)
                        firstVisible -= 1;

                    const std::size_t lastVisible = std::min(disassembly.size(), rowAtY(visibleBottom) + 1);

                    const ImVec2 itemSpacing = ImGui::GetStyle().ItemSpacing;
                    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(itemSpacing.x, 0.0F));

                    std::size_t anchorIndex = 0;
                    float anchorY = rowsOrigin.y;
                    bool hasAnchor = false;
                    for (std::size_t i = firstVisible; i < lastVisible; i += 1) {
                        const auto &instruction = disassembly[i];
                        ImGui::SetCursorPosY(contentStartY + rowTop(i));
                        const ImVec2 rowPosition = ImGui::GetCursorScreenPos();
                        anchorIndex = i;
                        anchorY = rowPosition.y;
                        hasAnchor = true;

                        ImGui::PushID(i);
                        if (ImGui::Selectable("##DisassemblyLine", selectedInstruction == i, ImGuiSelectableFlags_AllowOverlap, ImVec2(childWidth, rowHeight)))
                            ImHexApi::HexEditor::setSelection(m_imageBaseAddress.get(provider) + instruction.offset, instruction.size);
                        ImGui::PopID();

                        const float textY = rowPosition.y + ImGui::GetStyle().CellPadding.y;
                        drawList->AddText(ImVec2(rowAddressX, textY), ImGui::GetColorU32(ImGuiCol_Text),
                                          fmt::format("0x{0:X}", showOffsets ? instruction.offset : instruction.address).c_str());

                        std::string bytes = instruction.bytes;
                        if (instruction.size > 6 && bytes.size() > 14)
                            bytes = fmt::format("{} ...", bytes.substr(0, 14));
                        drawList->AddText(ImGui::GetFont(), ImGui::GetFontSize(), ImVec2(rowBytesX, textY), ImGui::GetColorU32(ImGuiCol_TextDisabled), bytes.data(), bytes.data() + bytes.size(), 0.0F, &bytesClip);

                        if (ImGui::IsMouseHoveringRect(ImVec2(rowBytesX, rowPosition.y), ImVec2(rowTypeIconX, rowPosition.y + rowHeight)) && !instruction.bytes.empty()) {
                            ImGui::BeginTooltip();
                            ImGui::TextUnformatted(instruction.bytes.c_str());
                            ImGui::EndTooltip();
                        }

                        if (instruction.type == ContentRegistry::Disassemblers::InstructionType::Call) {
                            const auto edge = std::ranges::find_if(flowEdges, [i](const auto &candidate) { return candidate.source == i; });
                            const ImVec2 nextRowPosition = ImGui::GetCursorScreenPos();

                            ImGui::SetCursorScreenPos(ImVec2(rowTypeIconX, rowPosition.y));

                            ImGui::PushID(i);
                            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
                            ImGui::BeginDisabled(edge == flowEdges.end());
                            if (ImGuiExt::DimmedIconButton(ICON_VS_DEBUG_STEP_OUT, ImGui::GetStyleColorVec4(ImGuiCol_Text), ImVec2(rowHeight, rowHeight)) && edge != flowEdges.end()) {
                                ImGui::SetScrollY(std::max(0.0F, contentStartY + rowTop(edge->target) - ImGui::GetWindowHeight() * 0.5F));
                                const auto &target = disassembly[edge->target];
                                ImHexApi::HexEditor::setSelection(m_imageBaseAddress.get(provider) + target.offset, target.size);
                            }
                            ImGui::EndDisabled();
                            ImGui::PopStyleVar();
                            ImGui::PopID();

                            if (edge != flowEdges.end() && ImGui::IsItemHovered()) {
                                ImGui::BeginTooltip();
                                ImGuiExt::TextFormatted("0x{:X}", disassembly[edge->target].address);
                                ImGui::EndTooltip();
                            }

                            ImGui::SetCursorScreenPos(nextRowPosition);
                        } else if (instruction.type == ContentRegistry::Disassemblers::InstructionType::Interrupt) {
                            const float iconX = rowTypeIconX + (rowHeight - ImGui::CalcTextSize(ICON_VS_PULSE).x) * 0.5F;
                            drawList->AddText(ImVec2(iconX, textY), ImGui::GetColorU32(ImGuiCol_PlotHistogram), ICON_VS_PULSE);
                        }

                        drawList->AddText(ImGui::GetFont(), ImGui::GetFontSize(), ImVec2(rowInstructionX, textY), ImU32(0xFFD69C56), instruction.mnemonic.data(), instruction.mnemonic.data() + instruction.mnemonic.size(), 0.0F, &instructionClip);

                        const float operandsX = rowInstructionX + ImGui::CalcTextSize(instruction.mnemonic.c_str()).x + ImGui::GetStyle().ItemSpacing.x;
                        if (operandsX < childMax.x)
                            drawList->AddText(ImGui::GetFont(), ImGui::GetFontSize(), ImVec2(operandsX, textY), ImGui::GetColorU32(ImGuiCol_Text), instruction.operators.data(), instruction.operators.data() + instruction.operators.size(), 0.0F, &instructionClip);

                        if (instruction.type == ContentRegistry::Disassemblers::InstructionType::Return) {
                            const float lineY = rowPosition.y + rowHeight + returnGap * 0.35F;
                            drawList->AddLine(ImVec2(rowsOrigin.x, lineY), ImVec2(childMax.x - ImGui::GetStyle().WindowPadding.x, lineY), ImGui::GetColorU32(ImGuiCol_Separator));
                        }
                    }

                    ImGui::SetCursorPosY(contentStartY + contentHeight);
                    ImGui::Dummy(ImVec2(1.0F, 1.0F));
                    ImGui::PopStyleVar();

                    drawList->PushClipRect(childMin, childMax, true);

                    const bool listingHovered = ImGui::IsWindowHovered();
                    const ImVec2 mousePosition = ImGui::GetMousePos();

                    const auto isSegmentHovered = [listingHovered, mousePosition](const ImVec2 &start, const ImVec2 &end) {
                        if (!listingHovered)
                            return false;

                        constexpr float HoverDistance = 4.0F;
                        const ImVec2 delta = end - start;
                        const float lengthSquared = delta.x * delta.x + delta.y * delta.y;
                        const float projection =
                            lengthSquared == 0.0F ?
                            0.0F : std::clamp(((mousePosition.x - start.x) * delta.x + (mousePosition.y - start.y) * delta.y) / lengthSquared, 0.0F, 1.0F);
                        const ImVec2 closest = start + delta * projection;
                        const float distanceX = mousePosition.x - closest.x;
                        const float distanceY = mousePosition.y - closest.y;
                        return distanceX * distanceX + distanceY * distanceY <= HoverDistance * HoverDistance;
                    };

                    const auto endpointOffset = [rowHeight](std::size_t slot, std::size_t count) {
                        if (count <= 1)
                            return 0.0F;

                        const float spacing = std::min(3.0F, rowHeight * 0.65F / float(count - 1));
                        return (float(slot) - float(count - 1) * 0.5F) * spacing;
                    };

                    for (const auto &edge : flowEdges) {
                        if (!hasAnchor)
                            break;

                        const float sourceY = anchorY + rowTop(edge.source) - rowTop(anchorIndex) + rowHeight * 0.5F +
                                              endpointOffset(edge.sourceSlot, edge.sourceSlotCount);
                        const float targetY = anchorY + rowTop(edge.target) - rowTop(anchorIndex) + rowHeight * 0.5F +
                                              endpointOffset(edge.targetSlot, edge.targetSlotCount);
                        if (std::max(sourceY, targetY) < childMin.y || std::min(sourceY, targetY) > childMax.y)
                            continue;

                        const float edgeX = rowAddressX - ImGui::GetFontSize() * 0.45F;
                        const bool overflow = edge.lane >= MaxVisibleFlowLanes;
                        const float laneX = overflow ? childMin.x - laneSpacing * float(edge.lane - MaxVisibleFlowLanes + 1) :
                                                      edgeX - edge.lane * laneSpacing - ImGui::GetFontSize() * 0.5F;
                        const ImVec2 source(edgeX, sourceY);
                        const ImVec2 sourceCorner(laneX, sourceY);
                        const ImVec2 targetCorner(laneX, targetY);
                        const ImVec2 target(edgeX, targetY);
                        const bool hovered = isSegmentHovered(source, sourceCorner) || isSegmentHovered(targetCorner, target) ||
                                             (!overflow && isSegmentHovered(sourceCorner, targetCorner));
                        const float hue = std::fmod(float(edge.target) * std::numbers::phi, 1.0F);
                        float red, green, blue;
                        ImGui::ColorConvertHSVtoRGB(hue, hovered ? 0.45F : 0.65F, 0.95F, red, green, blue);
                        const ImU32 color = ImGui::GetColorU32(ImVec4(red, green, blue, hovered ? 1.0F : 0.9F));
                        const float thickness = hovered ? 2.5F : 1.5F;
                        if (overflow) {
                            drawList->AddLine(source, sourceCorner, color, thickness);
                            drawList->AddLine(targetCorner, target, color, thickness);
                        } else {
                            const ImVec2 points[] = { source, sourceCorner, targetCorner, target };
                            drawList->AddPolyline(points, std::size(points), color, ImDrawFlags_None, thickness);
                        }
                        drawList->AddTriangleFilled(target, ImVec2(edgeX - 5.0F, targetY - 3.5F), ImVec2(edgeX - 5.0F, targetY + 3.5F), color);
                    }
                    drawList->PopClipRect();
                }
            }
            ImGui::EndChild();
        }
    }

    void ViewDisassembler::drawHelpText() {
        ImGuiExt::TextFormattedWrapped("This view lets you disassemble byte regions into assembly instructions of various different architectures.");
        ImGui::NewLine();
        ImGuiExt::TextFormattedWrapped("Select the desired Architecture from the tabs in the settings panel and configure its options as needed. Clicking the \"Disassemble\" button will disassemble the selected region (or the entire data if no region is selected) and display the resulting instructions in the listing below. Selecting an instruction is synchronized with the Hex Editor. Control-flow arrows connect direct jumps and calls to destinations within the listing. Call buttons navigate to their destination, while separators and event icons mark returns and interrupts.");
    }
}
