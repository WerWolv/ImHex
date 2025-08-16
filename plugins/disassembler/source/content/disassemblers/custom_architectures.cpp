#include <hex/api/content_registry/disassemblers.hpp>
#include <hex/helpers/default_paths.hpp>

#include <disasm/disasm.hpp>
#include <hex/helpers/fmt.hpp>
#include <hex/helpers/logger.hpp>
#include <wolv/utils/string.hpp>

namespace hex::plugin::disasm {

    class CustomArchitecture : public ContentRegistry::Disassemblers::Architecture {
    public:
        CustomArchitecture(std::string name, std::fs::path path) : Architecture(std::move(name)), m_path(std::move(path)) {}

        bool start() override {
            m_spec = ::disasm::spec::Loader::load(m_path, { m_path.parent_path() });
            
            return true;
        }

        void end() override {

        }

        void drawSettings() override {

        }

        std::optional<ContentRegistry::Disassemblers::Instruction> disassemble(u64 imageBaseAddress, u64 instructionLoadAddress, u64 instructionDataAddress, std::span<const u8> code) override {
            std::ignore = imageBaseAddress;
            std::ignore = instructionDataAddress;
            std::ignore = instructionLoadAddress;
            std::ignore = code;

            const auto instructions = m_spec.disassemble(code, 1);
            if (instructions.empty()) {
                return std::nullopt;
            }

            const auto &instruction = instructions.front();

            ContentRegistry::Disassemblers::Instruction disassembly = { };
            disassembly.address     = instructionDataAddress;
            disassembly.offset      = instructionDataAddress - imageBaseAddress;
            disassembly.size        = instruction.bytes.size();
            disassembly.mnemonic    = instruction.mnemonic;
            disassembly.operators   = instruction.operands;

            for (u8 byte : instruction.bytes)
                disassembly.bytes += fmt::format("{0:02X} ", byte);
            if (!disassembly.bytes.empty())
                disassembly.bytes.pop_back();

            return disassembly;
        }

    private:
        std::fs::path m_path;
        ::disasm::spec::Spec m_spec;

    };

    void registerCustomArchitectures() {
        for (const auto &folder : hex::paths::Disassemblers.all()) {
            if (!wolv::io::fs::exists(folder))
                    continue;

            for (const auto &entry : std::fs::directory_iterator(folder)) {
                try {
                    auto spec = ::disasm::spec::Loader::load(entry.path(), { entry.path().parent_path() });

                    ContentRegistry::Disassemblers::add<CustomArchitecture>(spec.getName(), entry.path());
                } catch (const std::exception &e) {
                    log::error("Failed to load disassembler config '{}': {}", wolv::util::toUTF8String(entry.path()), e.what());
                }
            }
        }
    }

}
