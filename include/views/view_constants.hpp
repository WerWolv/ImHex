#pragma once

#include <hex/views/view.hpp>

#include <cstdio>
#include <string>

namespace hex {

    enum class ConstantType {
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

    class ViewConstants : public View {
    public:
        explicit ViewConstants();
        ~ViewConstants() override;

        void drawContent() override;
        void drawMenu() override;

    private:
        void reloadConstants();

        std::vector<Constant> m_constants;
        std::vector<Constant*> m_filteredConstants;
        std::string m_search;
    };

}