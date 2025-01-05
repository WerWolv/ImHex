#include <hex/api/content_registry.hpp>
#include <hex/helpers/default_paths.hpp>

#include <disasm/disasm.hpp>
#include <hex/helpers/fmt.hpp>
#include <hex/helpers/logger.hpp>
#include <wolv/utils/string.hpp>

namespace hex::plugin::disasm {

    class CustomArchitecture : public ContentRegistry::Disassembler::Architecture {
    public:
        CustomArchitecture(::disasm::spec::Spec spec) : Architecture(spec.getName()), m_spec(std::move(spec)) {}

        bool start() override {
            return true;
        }

        void end() override {

        }

        void drawSettings() override {

        }

        std::optional<ContentRegistry::Disassembler::Instruction> disassemble(u64 imageBaseAddress, u64 instructionLoadAddress, u64 instructionDataAddress, std::span<const u8> code) override {
            std::ignore = imageBaseAddress;
            std::ignore = instructionDataAddress;
            std::ignore = instructionLoadAddress;
            std::ignore = code;

            auto instructions = m_spec.disassemble(code, 1);
            if (instructions.empty()) {
                return std::nullopt;
            }

            const auto &instruction = instructions.front();

            ContentRegistry::Disassembler::Instruction disassembly = { };
            disassembly.address     = instructionDataAddress;
            disassembly.offset      = instructionDataAddress - imageBaseAddress;
            disassembly.size        = instruction.bytes.size();
            disassembly.mnemonic    = instruction.mnemonic;
            disassembly.operators   = instruction.operands;

            for (u8 byte : instruction.bytes)
                disassembly.bytes += hex::format("{0:02X} ", byte);
            disassembly.bytes.pop_back();

            return disassembly;
        }

    private:
        ::disasm::spec::Spec m_spec;
    };

    void registerCustomArchitectures() {
        for (const auto &folder : hex::paths::Disassemblers.all()) {
            if (!wolv::io::fs::exists(folder))
                    continue;

            for (const auto &entry : std::fs::directory_iterator(folder)) {
                try {
                    auto spec = ::disasm::spec::Loader::load(entry.path(), { entry.path().parent_path() });

                    ContentRegistry::Disassembler::add<CustomArchitecture>(std::move(spec));
                } catch (const std::exception &e) {
                    log::error("Failed to load disassembler config '{}': {}", wolv::util::toUTF8String(entry.path()), e.what());
                }
            }
        }
    }

}
