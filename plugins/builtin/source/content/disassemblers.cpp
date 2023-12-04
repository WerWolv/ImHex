#include <imgui.h>
#include <hex/api/content_registry.hpp>

#include <capstone/capstone.h>
#include <hex/providers/provider.hpp>
#include <hex/ui/widgets.hpp>
#include <wolv/utils/guards.hpp>
#include <wolv/literals.hpp>

#include <ranges>

#include <ui/widgets.hpp>

namespace hex::plugin::builtin {

    namespace {

        using namespace ContentRegistry::Disassembler;
        using namespace wolv::literals;

        class ArchitectureCapstoneBase : public Architecture {
        public:
            ArchitectureCapstoneBase(const std::string &unlocalizedName, cs_arch arch) : Architecture(unlocalizedName), m_architecture(arch) { }

            virtual Instruction::Type getInstructionType(const cs_insn &instruction) {
                for (const auto &group : std::span { instruction.detail->groups, instruction.detail->groups_count }) {
                    if (group == CS_GRP_RET || group == CS_GRP_IRET)
                        return Instruction::Type::Return;
                    if (group == CS_GRP_CALL)
                        return Instruction::Type::Call;
                    if (group == CS_GRP_JUMP)
                        return Instruction::Type::Jump;
                }

                return Instruction::Type::Other;
            }

            std::vector<Instruction> disassemble(prv::Provider *provider, const Region &region, Task &task) override {
                std::vector<Instruction> disassembly;

                csh csHandle = {};
                if (cs_open(this->m_architecture, cs_mode(u64(this->m_mode) | (this->m_endian == 0 ? CS_MODE_LITTLE_ENDIAN : CS_MODE_BIG_ENDIAN)), &csHandle) != CS_ERR_OK)
                    return {};
                ON_SCOPE_EXIT { cs_close(&csHandle); };

                cs_option(csHandle, CS_OPT_SYNTAX,   this->m_syntax);
                cs_option(csHandle, CS_OPT_DETAIL,   CS_OPT_ON);
                cs_option(csHandle, CS_OPT_SKIPDATA, CS_OPT_ON);

                cs_insn *instruction = cs_malloc(csHandle);
                ON_SCOPE_EXIT { cs_free(instruction, 1); };

                std::vector<u8> bytes;
                u64 prevAddress = std::numeric_limits<u64>::max();
                for (u64 address = region.getStartAddress(); address < region.getEndAddress();) {
                    task.update(address - region.getStartAddress());
                    if (prevAddress == address)
                        break;
                    bytes.resize(std::min(2_MiB, (region.getEndAddress() - address) + 1));
                    provider->read(address, bytes.data(), bytes.size());

                    const u8 *code = bytes.data();
                    size_t size = bytes.size();
                    prevAddress = address;
                    while (cs_disasm_iter(csHandle, &code, &size, &address, instruction)) {
                        auto type = getInstructionType(*instruction);
                        auto line = Instruction {
                            .region    = { instruction->address, instruction->size },
                            .mnemonic  = instruction->mnemonic,
                            .operands  = instruction->op_str,
                            .type      = type,
                            .extraData = getExtraData(type, *instruction)
                        };

                        disassembly.emplace_back(std::move(line));
                    }
                }

                return disassembly;
            }

            void drawConfigInterface() override {
                ImGuiExt::BeginSubWindow("Endianess");
                {
                    drawRadioButtons(this->m_endian, {
                        { "Little", CS_MODE_LITTLE_ENDIAN },
                        { "Big",    CS_MODE_BIG_ENDIAN    }
                    });
                }
                ImGuiExt::EndSubWindow();

                ImGuiExt::BeginSubWindow("Syntax");
                {
                    drawRadioButtons(this->m_syntax, {
                        { "Intel",      CS_OPT_SYNTAX_INTEL     },
                        { "AT&T",       CS_OPT_SYNTAX_ATT       },
                        { "MASM",       CS_OPT_SYNTAX_MASM      },
                        { "Motorola",   CS_OPT_SYNTAX_MOTOROLA  }

                    });
                }
                ImGuiExt::EndSubWindow();
            }

            virtual std::optional<u64> getExtraData(Instruction::Type type, const cs_insn &instruction) = 0;

        protected:
            template<typename T>
            static void drawRadioButtons(T &currMode, const std::vector<std::pair<std::string, T>> &modes) {
                for (const auto &[index, mode] : modes | std::views::enumerate) {
                    const auto &[unlocalizedName, csMode] = mode;

                    if (ImGui::RadioButton(Lang(unlocalizedName), csMode == currMode)) {
                        currMode = csMode;
                    }

                    ImGui::SameLine();
                }
                ImGui::NewLine();
            }

            template<typename T>
            static void drawCheckbox(T &currMode, const std::string &unlocalizedName, T mode) {
                bool enabled = currMode & mode;
                if (ImGui::Checkbox(Lang(unlocalizedName), &enabled)) {
                    if (enabled)
                        currMode = T(u64(currMode) | u64(mode));
                    else
                        currMode = T(u64(currMode) & ~u64(mode));

                }
            }

        private:
            cs_arch m_architecture;
            cs_mode m_endian = cs_mode(0);
            cs_opt_value m_syntax = CS_OPT_SYNTAX_INTEL;

        protected:
            cs_mode m_mode = cs_mode(0);
        };


        class ArchitectureX86 : public ArchitectureCapstoneBase {
        public:
            ArchitectureX86() : ArchitectureCapstoneBase("x86", CS_ARCH_X86) {
                this->m_mode = CS_MODE_64;
            }

            std::optional<u64> getExtraData(Instruction::Type type, const cs_insn &instruction) override {
                switch (type) {
                    using enum Instruction::Type;

                    case Jump: {
                        // Get jump destination of jumps on x86
                        if (instruction.id == X86_INS_JMP) {
                            if (instruction.detail->x86.op_count != 1)
                                return std::nullopt;

                            const auto &op = instruction.detail->x86.operands[0];

                            if (op.type == X86_OP_IMM)
                                return op.imm;

                            if (op.type == X86_OP_MEM && op.mem.base == X86_REG_RIP)
                                return instruction.address + instruction.size + op.mem.disp;
                        }

                        // Get jump destination of conditional jumps on x86
                        if (instruction.id >= X86_INS_JAE && instruction.id <= X86_INS_JLE) {
                            if (instruction.detail->x86.op_count != 1)
                                return std::nullopt;

                            const auto &op = instruction.detail->x86.operands[0];

                            if (op.type == X86_OP_IMM)
                                return op.imm;

                            if (op.type == X86_OP_MEM && op.mem.base == X86_REG_RIP)
                                return instruction.address + instruction.size + op.mem.disp;
                        }

                        break;
                    }
                    case Call: {
                        if (instruction.id == X86_INS_CALL) {
                            if (instruction.detail->x86.op_count != 1)
                                return std::nullopt;

                            const auto &op = instruction.detail->x86.operands[0];

                            if (op.type == X86_OP_IMM)
                                return op.imm;

                            if (op.type == X86_OP_MEM && op.mem.base == X86_REG_RIP)
                                return instruction.address + instruction.size + op.mem.disp;
                        }

                        break;
                    }
                    case Return:
                        break;
                    case Other:
                        break;
                }


                return std::nullopt;
            }

            void drawConfigInterface() override {
                ArchitectureCapstoneBase::drawConfigInterface();

                ImGuiExt::BeginSubWindow("Address Width");
                {
                    drawRadioButtons(this->m_mode, {
                        { "16 Bit", CS_MODE_16 },
                        { "32 Bit", CS_MODE_32 },
                        { "64 Bit", CS_MODE_64 },
                    });
                }
                ImGuiExt::EndSubWindow();
            }
        };

        class ArchitectureARM32 : public ArchitectureCapstoneBase {
        public:
            ArchitectureARM32() : ArchitectureCapstoneBase("ARM", CS_ARCH_ARM) {
                this->m_mode = CS_MODE_ARM;
            }

            std::optional<u64> getExtraData(Instruction::Type type, const cs_insn &instruction) override {
                switch (type) {
                    using enum Instruction::Type;

                    case Jump: {
                        // Get jump destination of jumps on ARM
                        if (instruction.id == ARM_INS_B) {
                            if (instruction.detail->arm.op_count != 1)
                                return std::nullopt;

                            const auto &op = instruction.detail->arm.operands[0];

                            if (op.type == ARM_OP_IMM)
                                return op.imm;

                            if (op.type == ARM_OP_MEM && op.mem.base == ARM_REG_PC)
                                return instruction.address + instruction.size + op.mem.disp;
                        }

                        break;
                    }
                    case Call: {
                        if (instruction.id == ARM_INS_BL) {
                            if (instruction.detail->arm.op_count != 1)
                                return std::nullopt;

                            const auto &op = instruction.detail->arm.operands[0];

                            if (op.type == ARM_OP_IMM)
                                return op.imm;

                            if (op.type == ARM_OP_MEM && op.mem.base == ARM_REG_PC)
                                return instruction.address + instruction.size + op.mem.disp;
                        }

                        break;
                    }
                    case Return:
                        break;
                    case Other:
                        break;
                }


                return std::nullopt;
            }

            void drawConfigInterface() override {
                ArchitectureCapstoneBase::drawConfigInterface();

                ImGuiExt::BeginSubWindow("Instruction Set");
                {
                    drawRadioButtons(this->m_mode, {
                        { "ARM", CS_MODE_ARM },
                        { "Thumb & Thumb-2", CS_MODE_THUMB },
                        { "ARMv8 / AArch32", CS_MODE_THUMB }
                    });

                    drawCheckbox(this->m_mode, "Cortex-M", CS_MODE_MCLASS);
                }
                ImGuiExt::EndSubWindow();
            }
        };

        class ArchitectureARM64 : public ArchitectureCapstoneBase {
        public:
            ArchitectureARM64() : ArchitectureCapstoneBase("ARM64 / AArch64", CS_ARCH_ARM64) {
            }

            std::optional<u64> getExtraData(Instruction::Type type, const cs_insn &instruction) override {
                switch (type) {
                    using enum Instruction::Type;

                    case Jump: {
                        // Get jump destination of jumps on ARM64
                        if (instruction.id == ARM64_INS_B) {
                            if (instruction.detail->arm64.op_count != 1)
                                return std::nullopt;

                            const auto &op = instruction.detail->arm64.operands[0];

                            if (op.type == ARM64_OP_IMM)
                                return op.imm;

                            if (op.type == ARM64_OP_MEM)
                                return instruction.address + instruction.size + op.mem.disp;
                        }

                        break;
                    }
                    case Call: {
                        if (instruction.id == ARM64_INS_BL) {
                            if (instruction.detail->arm64.op_count != 1)
                                return std::nullopt;

                            const auto &op = instruction.detail->arm64.operands[0];

                            if (op.type == ARM64_OP_IMM)
                                return op.imm;

                            if (op.type == ARM64_OP_MEM)
                                return instruction.address + instruction.size + op.mem.disp;
                        }

                        break;
                    }
                    case Return:
                        break;
                    case Other:
                        break;
                }



                return std::nullopt;
            }
        };

    }

    void registerDisassemblers() {
        ContentRegistry::Disassembler::add<ArchitectureX86>();
        ContentRegistry::Disassembler::add<ArchitectureARM32>();
        ContentRegistry::Disassembler::add<ArchitectureARM64>();
    }

}
