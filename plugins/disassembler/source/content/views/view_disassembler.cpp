#include "content/views/view_disassembler.hpp"
#include "hex/api/content_registry.hpp"

#include <hex/providers/provider.hpp>
#include <hex/helpers/fmt.hpp>

#include <fonts/vscode_icons.hpp>

#include <cstring>
#include <toasts/toast_notification.hpp>

#include <wolv/literals.hpp>

using namespace std::literals::string_literals;
using namespace wolv::literals;

namespace hex::plugin::disasm {

    ViewDisassembler::ViewDisassembler() : View::Window("hex.disassembler.view.disassembler.name", ICON_VS_FILE_CODE) {
        EventProviderDeleted::subscribe(this, [this](const auto*) {
            m_disassembly.clear();
        });

        ContentRegistry::Interface::addMenuItem({ "hex.builtin.menu.edit", "hex.builtin.menu.edit.disassemble_range" }, ICON_VS_DEBUG_LINE_BY_LINE, 3100, CTRLCMD + SHIFT + Keys::D, [this] {
            ImGui::SetWindowFocus(this->getName().c_str());
            this->getWindowOpenState() = true;

            m_range = ui::RegionType::Region;
            m_regionToDisassemble = ImHexApi::HexEditor::getSelection()->getRegion();

            this->disassemble();
        }, [this]{
            return ImHexApi::HexEditor::isSelectionValid() && !this->m_disassemblerTask.isRunning();
        });
    }

    ViewDisassembler::~ViewDisassembler() {
        EventDataChanged::unsubscribe(this);
        EventRegionSelected::unsubscribe(this);
        EventProviderDeleted::unsubscribe(this);
    }

    void ViewDisassembler::disassemble() {
        m_disassembly.clear();

        if (m_regionToDisassemble.getStartAddress() < m_imageBaseAddress)
            return;

        m_disassemblerTask = TaskManager::createTask("hex.disassembler.view.disassembler.disassembling"_lang, m_regionToDisassemble.getSize(), [this](auto &task) {
            // Create a capstone disassembler instance
            if (m_currArchitecture->start()) {
                auto provider = ImHexApi::Provider::get();
                std::vector<u8> buffer(1_MiB, 0x00);

                const u64 codeOffset = m_regionToDisassemble.getStartAddress() - m_imageBaseAddress;

                // Read the data in chunks and disassemble it
                u64 instructionLoadAddress = m_imageLoadAddress + codeOffset;
                u64 instructionDataAddress = m_regionToDisassemble.getStartAddress();

                bool hadError = false;
                while (instructionDataAddress < m_regionToDisassemble.getEndAddress()) {
                    // Read a chunk of data
                    size_t bufferSize = std::min<u64>(buffer.size(), (m_regionToDisassemble.getEndAddress() - instructionDataAddress));
                    provider->read(instructionDataAddress, buffer.data(), bufferSize);

                    auto code = std::span(buffer.data(), bufferSize);

                    // Ask capstone to disassemble the data
                    while (true) {
                        auto instruction = m_currArchitecture->disassemble(m_imageBaseAddress, instructionLoadAddress, instructionDataAddress, code);
                        if (!instruction.has_value())
                            break;

                        task.update(instructionDataAddress);

                        m_disassembly.push_back(instruction.value());

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

                m_currArchitecture->end();
            }
        });
    }

    void ViewDisassembler::exportToFile() {
        TaskManager::createTask("hex.ui.common.processing"_lang, TaskManager::NoProgress, [this](auto &) {
            TaskManager::doLater([this] {
                fs::openFileBrowser(fs::DialogMode::Save, {}, [this](const std::fs::path &path) {
                    auto p = path;
                    if (p.extension() != ".asm")
                        p.replace_filename(hex::format("{}{}", p.filename().string(), ".asm"));
                    auto file = wolv::io::File(p, wolv::io::File::Mode::Create);

                    if (!file.isValid()) {
                        ui::ToastError::open("hex.disassembler.view.disassembler.export.popup.error"_lang);
                        return;
                    }

                    // As disassembly code can be quite long, we prefer writing each disassembled instruction to file
                    for (const ContentRegistry::Disassembler::Instruction& instruction : m_disassembly) {
                        // We test for a "bugged" case that should never happen - the instruction should always have a mnemonic
                        if (instruction.mnemonic.empty())
                            continue;

                        if (instruction.operators.empty())
                            file.writeString(hex::format("{}\n", instruction.mnemonic));
                        else
                            file.writeString(hex::format("{} {}\n", instruction.mnemonic, instruction.operators));
                    }
                });
            });
        });
    }

    void ViewDisassembler::drawContent() {
        auto provider = ImHexApi::Provider::get();
        if (ImHexApi::Provider::isValid() && provider->isReadable()) {
            // Draw region selection picker
            ui::regionSelectionPicker(&m_regionToDisassemble, provider, &m_range, true, true);

            ImGuiExt::Header("hex.disassembler.view.disassembler.position"_lang);

            // Draw base address input
            ImGuiExt::InputHexadecimal("hex.disassembler.view.disassembler.image_load_address"_lang, &m_imageLoadAddress, ImGuiInputTextFlags_CharsHexadecimal);
            ImGui::SameLine();
            ImGuiExt::HelpHover("hex.disassembler.view.disassembler.image_load_address.hint"_lang, ICON_VS_INFO);

            // Draw code region start address input
            ImGui::BeginDisabled(m_range == ui::RegionType::EntireData);
            {
                ImGuiExt::InputHexadecimal("hex.disassembler.view.disassembler.image_base_address"_lang, &m_imageBaseAddress, ImGuiInputTextFlags_CharsHexadecimal);
                ImGui::SameLine();
                ImGuiExt::HelpHover("hex.disassembler.view.disassembler.image_base_address.hint"_lang, ICON_VS_INFO);
            }
            ImGui::EndDisabled();

            // Draw settings
            {
                ImGuiExt::Header("hex.ui.common.settings"_lang);

                // Draw architecture selector
                const auto &architectures = ContentRegistry::Disassembler::impl::getArchitectures();
                if (architectures.empty()) {
                    ImGuiExt::TextSpinner("hex.disassembler.view.disassembler.arch"_lang);
                } else {
                    if (m_currArchitecture == nullptr) {
                        m_currArchitecture = architectures.begin()->second();
                    }

                    if (ImGui::BeginCombo("hex.disassembler.view.disassembler.arch"_lang, m_currArchitecture->getName().c_str())) {
                        for (const auto &[name, creator] : architectures) {
                            if (ImGui::Selectable(name.c_str(), name == m_currArchitecture->getName())) {
                                m_currArchitecture = creator();
                            }
                        }
                        ImGui::EndCombo();
                    }

                    // Draw sub-settings for each architecture
                    if (ImGuiExt::BeginBox()) {
                        m_currArchitecture->drawSettings();
                    }
                    ImGuiExt::EndBox();
                }
            }

            // Draw disassemble button
            ImGui::BeginDisabled(m_disassemblerTask.isRunning() || m_regionToDisassemble.getStartAddress() < m_imageBaseAddress);
            {
                if (ImGuiExt::DimmedButton("hex.disassembler.view.disassembler.disassemble"_lang))
                    this->disassemble();
            }
            ImGui::EndDisabled();

            // Draw export to file icon button
            ImGui::SameLine();
            ImGui::BeginDisabled(m_disassemblerTask.isRunning() || m_disassembly.empty());
            {
                if (ImGuiExt::DimmedIconButton(ICON_VS_EXPORT, ImGui::GetStyleColorVec4(ImGuiCol_Text)))
                    this->exportToFile();
            }
            ImGui::EndDisabled();
            ImGuiExt::InfoTooltip("hex.disassembler.view.disassembler.export"_lang);

            // Draw a spinner if the disassembler is running
            if (m_disassemblerTask.isRunning()) {
                ImGuiExt::TextSpinner("hex.disassembler.view.disassembler.disassembling"_lang);
            }

            ImGui::NewLine();

            ImGui::TextUnformatted("hex.disassembler.view.disassembler.disassembly.title"_lang);
            ImGui::Separator();

            // Draw disassembly table
            if (ImGui::BeginTable("##disassembly", 4, ImGuiTableFlags_ScrollY | ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable)) {
                ImGui::TableSetupScrollFreeze(0, 1);
                ImGui::TableSetupColumn("hex.disassembler.view.disassembler.disassembly.address"_lang);
                ImGui::TableSetupColumn("hex.disassembler.view.disassembler.disassembly.offset"_lang);
                ImGui::TableSetupColumn("hex.disassembler.view.disassembler.disassembly.bytes"_lang);
                ImGui::TableSetupColumn("hex.disassembler.view.disassembler.disassembly.title"_lang);

                if (!m_disassemblerTask.isRunning()) {
                    ImGuiListClipper clipper;
                    clipper.Begin(m_disassembly.size());

                    ImGui::TableHeadersRow();
                    while (clipper.Step()) {
                        for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
                            const auto &instruction = m_disassembly[i];

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

}
