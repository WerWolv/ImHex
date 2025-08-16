#pragma once

#include <hex.hpp>

#include <hex/api/localization_manager.hpp>

#include <string>
#include <map>
#include <memory>
#include <functional>
#include <optional>
#include <span>

EXPORT_MODULE namespace hex {

    /* Disassembler Registry. Allows adding new disassembler architectures */
    namespace ContentRegistry::Disassemblers {

        struct Instruction {
            u64 address;
            u64 offset;
            size_t size;
            std::string bytes;
            std::string mnemonic;
            std::string operators;
        };

        class Architecture {
        public:
            explicit Architecture(std::string name) : m_name(std::move(name)) {}
            virtual ~Architecture() = default;

            virtual bool start() = 0;
            virtual void end() = 0;

            virtual std::optional<Instruction> disassemble(u64 imageBaseAddress, u64 instructionLoadAddress, u64 instructionDataAddress, std::span<const u8> code) = 0;
            virtual void drawSettings() = 0;

            [[nodiscard]] const std::string& getName() const { return m_name; }

        private:
            std::string m_name;
        };

        namespace impl {

            using CreatorFunction = std::function<std::unique_ptr<Architecture>()>;

            void addArchitectureCreator(CreatorFunction function);

            const std::map<std::string, CreatorFunction>& getArchitectures();

        }

        template<std::derived_from<Architecture> T>
        void add(auto && ...args) {
            impl::addArchitectureCreator([...args = std::move(args)] {
                return std::make_unique<T>(args...);
            });
        }

    }

}