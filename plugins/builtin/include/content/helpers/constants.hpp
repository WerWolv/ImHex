#pragma once

#include <string>
#include <vector>

#include <hex/helpers/binary_pattern.hpp>

namespace hex::plugin::builtin {

    struct Constant {
        std::string name;
        std::string description;
        BinaryPattern value;
    };

    class ConstantGroup {
    public:
        explicit ConstantGroup(const std::fs::path &path);

        [[nodiscard]] const std::string& getName() const { return m_name; }
        const std::vector<Constant>& getConstants() const { return m_constants; }
    private:
        std::string m_name;
        std::vector<Constant> m_constants;
    };

}