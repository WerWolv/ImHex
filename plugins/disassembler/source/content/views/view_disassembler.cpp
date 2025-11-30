#include <content/views/view_disassembler.hpp>
#include <hex/api/content_registry/user_interface.hpp>
#include <hex/api/content_registry/views.hpp>
#include <hex/api/imhex_api/hex_editor.hpp>

#include <hex/providers/provider.hpp>
#include <hex/helpers/fmt.hpp>

#include <fonts/vscode_icons.hpp>
#include <imgui_internal.h>

#include <toasts/toast_notification.hpp>

#include <wolv/literals.hpp>

using namespace std::literals::string_literals;
using namespace wolv::literals;

namespace hex::plugin::disasm {

    ViewDisassembler::ViewDisassembler() : View::Window("hex.disassembler.view.disassembler.name", ICON_VS_FILE_CODE) {
        EventProviderDeleted::subscribe(this, [this](const auto *provider) {
            m_disassembly.get(provider).clear();
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
    }

    void ViewDisassembler::disassemble() {
        const auto provider = ImHexApi::Provider::get();

        m_disassembly.get(provider).clear();

        if (m_regionToDisassemble.get(provider).getStartAddress() < m_imageBaseAddress)
            return;

        m_disassemblerTask = TaskManager::createTask("hex.disassembler.view.disassembler.disassembling", m_regionToDisassemble.get(provider).getSize(), [this, provider](auto &task) {
            const auto &currArchitecture = m_currArchitecture.get(provider);
            const auto region = m_regionToDisassemble.get(provider);
            auto &disassembly = m_disassembly.get(provider);

            if (currArchitecture == nullptr)
                return;

            // Create a capstone disassembler instance
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
                    size_t bufferSize = std::min<u64>(buffer.size(), (region.getEndAddress()-instructionDataAddress)+1);
                    provider->read(instructionDataAddress, buffer.data(), bufferSize);

                    auto code = std::span(buffer.data(), bufferSize);

                    // Ask capstone to disassemble the data
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
                            if (ImGuiExt::BeginBox()) {
                                currArchitecture->drawSettings();
                            }
                            ImGuiExt::EndBox();
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

            // Draw disassembly table
            if (ImGui::BeginTable("##disassembly", 4, ImGuiTableFlags_ScrollY | ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable)) {
                ImGui::TableSetupScrollFreeze(0, 1);
                ImGui::TableSetupColumn("hex.disassembler.view.disassembler.disassembly.address"_lang);
                ImGui::TableSetupColumn("hex.disassembler.view.disassembler.disassembly.offset"_lang);
                ImGui::TableSetupColumn("hex.disassembler.view.disassembler.disassembly.bytes"_lang);
                ImGui::TableSetupColumn("hex.disassembler.view.disassembler.disassembly.title"_lang);

                if (!m_disassemblerTask.isRunning()) {
                    ImGuiListClipper clipper;
                    clipper.Begin(disassembly.size());

                    ImGui::TableHeadersRow();
                    while (clipper.Step()) {
                        for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
                            const auto &instruction = disassembly[i];

                            ImGui::TableNextRow();
                            ImGui::TableNextColumn();

                            // Draw a selectable label for the address
                            ImGui::PushID(i);
                            if (ImGui::Selectable("##DisassemblyLine", false, ImGuiSelectableFlags_SpanAllColumns)) {
                                ImHexApi::HexEditor::setSelection(instruction.offset, instruction.size);
                            }
                            ImGui::PopID();

                            // Draw instruction address
                            ImGui::SameLine();
                            ImGuiExt::TextFormatted("0x{0:X}", instruction.address);

                            // Draw instruction offset
                            ImGui::TableNextColumn();
                            ImGuiExt::TextFormatted("0x{0:X}", instruction.offset);

                            // Draw instruction bytes
                            ImGui::TableNextColumn();
                            ImGui::TextUnformatted(instruction.bytes.c_str());

                            // Draw instruction mnemonic and operands
                            ImGui::TableNextColumn();
                            ImGuiExt::TextFormattedColored(ImColor(0xFFD69C56), "{}", instruction.mnemonic);
                            ImGui::SameLine();
                            ImGui::TextUnformatted(instruction.operators.c_str());
                        }
                    }

                    clipper.End();
                }

                ImGui::EndTable();
            }
        }
    }

    void ViewDisassembler::drawHelpText() {
        ImGuiExt::TextFormattedWrapped("This view lets you disassemble byte regions into assembly instructions of various different architectures.");
        ImGui::NewLine();
        ImGuiExt::TextFormattedWrapped("Select the desired Architecture from the tabs in the settings panel and configure its options as needed. Clicking the \"Disassemble\" button will disassemble the selected region (or the entire data if no region is selected) and display the resulting instructions in a table below.");
    }
}
