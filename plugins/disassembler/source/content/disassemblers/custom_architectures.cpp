#include <hex/api/content_registry/disassemblers.hpp>

#include <disasm/disasm.hpp>
#include <hex/helpers/fmt.hpp>
#include <hex/helpers/logger.hpp>
#include <hex/helpers/default_paths.hpp>
#include <wolv/utils/string.hpp>

#include <hex/ui/imgui_imhex_extensions.h>
#include <imgui.h>

namespace hex::plugin::disasm {

    namespace {

        struct SettingVisitorGui : ::disasm::spec::Setting::Visitor {
            SettingVisitorGui() = default;
            explicit SettingVisitorGui(std::map<std::string, i64> *variables) : m_variables(variables) {}

            void visit(::disasm::spec::SettingBool& setting) override {
                bool value = (*m_variables)[setting.name] != 0;
                ImGui::Checkbox(setting.displayName.c_str(), &value);
                (*m_variables)[setting.name] = value ? 1 : 0;
            }

            void visit(::disasm::spec::SettingInt& setting) override {
                i64 value = (*m_variables).try_emplace(setting.name, setting.min).first->second;
                switch (setting.radix) {
                    using enum ::disasm::spec::SettingInt::Radix;
                    case Decimal: {
                        if (ImGui::InputScalar(setting.displayName.c_str(), ImGuiDataType_S64, &value, nullptr, nullptr, "%lld")) {
                            if (value < setting.min) value = setting.min;
                            if (value > setting.max) value = setting.max;
                            (*m_variables)[setting.name] = value;
                        }
                        break;
                    }
                    case Hexadecimal: {
                        if (ImGuiExt::InputHexadecimal(setting.displayName.c_str(), reinterpret_cast<u64*>(&value))) {
                            if (value < setting.min) value = setting.min;
                            if (value > setting.max) value = setting.max;
                            (*m_variables)[setting.name] = value;
                        }
                        break;
                    }
                }
            }

            void visit(::disasm::spec::SettingEnum& setting) override {
                auto &value = (*m_variables)[setting.name];
                int selection = 0;
                for (const auto &enumValue : setting.values) {
                    if (value == enumValue.value)
                        break;
                    selection += 1;
                }
                if (size_t(selection) >= setting.values.size())
                    selection = 0;

                std::vector<std::string> options;
                for (const auto &option : setting.values) {
                    options.emplace_back(fmt::format("{}:  {}", setting.displayName, option.name));
                }

                ImGui::SliderInt("##endian", &selection, 0, options.size() - 1, options[selection].c_str(), ImGuiSliderFlags_NoInput);

                value = setting.values[selection].value;
            }

            std::map<std::string, i64> *m_variables = nullptr;
        };

    }

    class CustomArchitecture : public ContentRegistry::Disassemblers::Architecture {
    public:
        CustomArchitecture(std::string name, std::fs::path path) : Architecture(std::move(name)), m_path(std::move(path)), m_visitor(&m_variables) {
            m_spec = ::disasm::spec::Loader::load(m_path, { m_path.parent_path() });
        }

        bool start() override {
            std::scoped_lock lock(m_mutex);
            m_spec = ::disasm::spec::Loader::load(m_path, { m_path.parent_path() });

            return true;
        }

        void end() override {

        }

        void drawSettings() override {
            std::scoped_lock lock(m_mutex);
            for (const auto &setting : m_spec.getSettings()) {
                setting->accept(m_visitor);
            }
        }

        bool hasSettings() override {
            return !m_spec.getSettings().empty();
        }

        std::optional<ContentRegistry::Disassemblers::Instruction> disassemble(u64 imageBaseAddress, u64 instructionLoadAddress, u64 instructionDataAddress, std::span<const u8> code) override {
            std::ignore = imageBaseAddress;
            std::ignore = instructionDataAddress;
            std::ignore = instructionLoadAddress;
            std::ignore = code;

            const auto instructions = m_spec.disassemble(code, 1, m_variables);
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

        std::string getFormattedPatternLanguageType(u64 imageBaseAddress, u64 instructionLoadAddress) override {
            std::ignore = imageBaseAddress;
            std::ignore = instructionLoadAddress;

            return "Unsupported";
        }

    private:
        std::fs::path m_path;
        ::disasm::spec::Spec m_spec;
        SettingVisitorGui m_visitor;
        std::map<std::string, i64> m_variables;
        std::mutex m_mutex;
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
