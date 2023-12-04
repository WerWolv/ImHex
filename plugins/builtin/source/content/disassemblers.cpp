#include <imgui.h>
#include <hex/api/content_registry.hpp>

#include <capstone/capstone.h>
#include <hex/providers/provider.hpp>
#include <wolv/utils/guards.hpp>
#include <wolv/literals.hpp>

namespace hex::plugin::builtin {

    namespace {

        using namespace ContentRegistry::Disassembler;
        using namespace wolv::literals;

        class ArchitectureCapstoneBase : public Architecture {
        public:
            ArchitectureCapstoneBase(const std::string &unlocalizedName, cs_arch arch) : Architecture(unlocalizedName), m_architecture(arch) { }

            std::vector<Instruction> disassemble(prv::Provider *provider, const Region &region) override {
                std::vector<Instruction> disassembly;

                csh csHandle = {};
                if (cs_open(this->m_architecture, CS_MODE_64, &csHandle) != CS_ERR_OK)
                    return {};
                ON_SCOPE_EXIT { cs_close(&csHandle); };

                cs_option(csHandle, CS_OPT_SYNTAX,   CS_OPT_SYNTAX_INTEL);
                cs_option(csHandle, CS_OPT_DETAIL,   CS_OPT_ON);
                cs_option(csHandle, CS_OPT_SKIPDATA, CS_OPT_ON);

                cs_insn *instruction = cs_malloc(csHandle);
                ON_SCOPE_EXIT { cs_free(instruction, 1); };

                std::vector<u8> bytes;
                u64 prevAddress = std::numeric_limits<u64>::max();
                for (u64 address = region.getStartAddress(); address < region.getEndAddress();) {
                    if (prevAddress == address)
                        break;
                    bytes.resize(std::min(2_MiB, (region.getEndAddress() - address) + 1));
                    provider->read(address, bytes.data(), bytes.size());

                    const u8 *code = bytes.data();
                    size_t size = bytes.size();
                    prevAddress = address;
                    while (cs_disasm_iter(csHandle, &code, &size, &address, instruction)) {
                        auto line = Instruction {
                            .region = { instruction->address, instruction->size },
                            .mnemonic = instruction->mnemonic,
                            .operands = instruction->op_str,
                            .jumpDestination = getJumpDestination(*instruction)
                        };

                        disassembly.emplace_back(std::move(line));
                    }
                }

                return disassembly;
            }

            void drawConfigInterface() override {
                ImGui::TextUnformatted("Config Interface");
            }

            virtual std::optional<u64> getJumpDestination(const cs_insn &instruction) = 0;

        private:
            cs_arch m_architecture;
        };


        class ArchitectureX86 : public ArchitectureCapstoneBase {
        public:
            ArchitectureX86() : ArchitectureCapstoneBase("x86", CS_ARCH_X86) { }

            std::optional<u64> getJumpDestination(const cs_insn &instruction) override {
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

                return std::nullopt;
            }
        };

    }

    void registerDisassemblers() {
        ContentRegistry::Disassembler::add<ArchitectureX86>();
    }

}
