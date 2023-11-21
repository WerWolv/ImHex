#pragma once

#include <hex/ui/view.hpp>

#include <string>

namespace hex::plugin::builtin {

    enum class ConstantType
    {
        Int10,
        Int16BigEndian,
        Int16LittleEndian
    };

    struct Constant {
        std::string name, description;
        std::string category;
        ConstantType type;
        std::string value;
    };

    class ViewConstants : public View::Window {
    public:
        explicit ViewConstants();
        ~ViewConstants() override = default;

        void drawContent() override;

    private:
        void reloadConstants();

        std::vector<Constant> m_constants;
        std::vector<size_t> m_filterIndices;
        std::string m_filter;
    };

}