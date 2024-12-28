#include <hex/api/imhex_api.hpp>
#include <hex/api/content_registry.hpp>

#include <hex/helpers/http_requests.hpp>
#include <hex/helpers/utils.hpp>
#include <hex/helpers/fmt.hpp>

#include <pl/core/evaluator.hpp>

#include <pl/patterns/pattern.hpp>

#include <capstone/capstone.h>
#include <content/helpers/disassembler.hpp>


namespace hex::plugin::disasm {

    class PatternInstruction : public pl::ptrn::Pattern {
    public:
        PatternInstruction(pl::core::Evaluator *evaluator, u64 offset, size_t size, u32 line)
            : Pattern(evaluator, offset, size, line) { }

        [[nodiscard]] std::unique_ptr<Pattern> clone() const override {
            return std::unique_ptr<Pattern>(new PatternInstruction(*this));
        }

        [[nodiscard]] std::string getFormattedName() const override {
            return this->getTypeName();
        }
        [[nodiscard]] bool operator==(const Pattern &other) const override { return compareCommonProperties<decltype(*this)>(other); }
        void accept(pl::PatternVisitor &v) override {
            v.visit(*this);
        }

        std::vector<pl::u8> getRawBytes() override {
            std::vector<u8> result;
            result.resize(this->getSize());

            this->getEvaluator()->readData(this->getOffset(), result.data(), result.size(), this->getSection());
            if (this->getEndian() != std::endian::native)
                std::reverse(result.begin(), result.end());

            return result;
        }

        void setInstructionString(std::string instructionString) {
            m_instructionString = std::move(instructionString);
        }

    protected:
        [[nodiscard]] std::string formatDisplayValue() override {
            return m_instructionString;
        }

    private:
        std::string m_instructionString;
    };

    void registerPatternLanguageTypes() {
        using namespace pl::core;
        using FunctionParameterCount = pl::api::FunctionParameterCount;

        {
            const pl::api::Namespace nsHexDec = { "builtin", "hex", "dec" };

            /* Json<data_pattern> */
            ContentRegistry::PatternLanguage::addType(nsHexDec, "Instruction", FunctionParameterCount::exactly(4), [](Evaluator *evaluator, auto params) -> std::unique_ptr<pl::ptrn::Pattern> {
                cs_arch arch;
                cs_mode mode;

                try {
                    std::tie(arch, mode) = CapstoneDisassembler::stringToSettings(params[0].toString());
                } catch (const std::exception &e) {
                    err::E0012.throwError(e.what());
                }
                const auto syntaxString = params[1].toString();
                const auto imageBaseAddress = params[2].toUnsigned();
                const auto imageLoadAddress = params[3].toUnsigned();

                const auto address = evaluator->getReadOffset();

                const auto codeOffset = address - imageBaseAddress;

                u64 instructionLoadAddress = imageLoadAddress + codeOffset;

                csh capstone;
                if (cs_open(arch, mode, &capstone) == CS_ERR_OK) {
                    cs_opt_value syntax;
                    if (equalsIgnoreCase(syntaxString, "intel"))
                        syntax = CS_OPT_SYNTAX_INTEL;
                    else if (equalsIgnoreCase(syntaxString, "at&t"))
                        syntax = CS_OPT_SYNTAX_ATT;
                    else if (equalsIgnoreCase(syntaxString, "masm"))
                        syntax = CS_OPT_SYNTAX_MASM;
                    else if (equalsIgnoreCase(syntaxString, "motorola"))
                        syntax = CS_OPT_SYNTAX_MOTOROLA;
                    else
                        err::E0012.throwError(hex::format("Invalid disassembler syntax name '{}'", syntaxString));

                    cs_option(capstone, CS_OPT_SYNTAX, syntax);
                    cs_option(capstone, CS_OPT_SKIPDATA, CS_OPT_ON);

                    const auto sectionId = evaluator->getSectionId();
                    std::vector<u8> data(std::min<u64>(32, evaluator->getSectionSize(sectionId) - address));
                    evaluator->readData(address, data.data(), data.size(), sectionId);

                    auto *instruction = cs_malloc(capstone);
                    ON_SCOPE_EXIT { cs_free(instruction, 1); };

                    const u8 *code = data.data();
                    size_t dataSize = data.size();
                    if (!cs_disasm_iter(capstone, &code, &dataSize, &instructionLoadAddress, instruction)) {
                        err::E0012.throwError("Failed to disassemble instruction");
                    }

                    auto result = std::make_unique<PatternInstruction>(evaluator, address, instruction->size, 0);

                    std::string instructionString;
                    if (instruction->mnemonic[0] != '\x00')
                        instructionString += instruction->mnemonic;
                    if (instruction->op_str[0] != '\x00') {
                        instructionString += ' ';
                        instructionString += instruction->op_str;
                    }
                    result->setInstructionString(instructionString);

                    cs_close(&capstone);

                    return result;
                } else {
                    err::E0012.throwError("Failed to disassemble instruction");
                }
            });
        }
    }
}
