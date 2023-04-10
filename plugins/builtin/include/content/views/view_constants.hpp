#pragma once

#include <hex/ui/view.hpp>

#include <cstdio>
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

    class ViewConstants : public View {
    public:
        explicit ViewConstants();
        ~ViewConstants() override = default;

        void drawContent() override;

        ImVec2 getMinSize() const override {
            return scaled(ImVec2(300, 400));
        }

        ImVec2 getMaxSize() const override {
            return { FLT_MAX, 800_scaled };
        }

    private:
        void reloadConstants();

        std::vector<Constant> m_constants;
        std::vector<size_t> m_filterIndices;
        std::string m_filter;
    };

}