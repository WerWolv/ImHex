#pragma once

#include <hex.hpp>

#include <string>

namespace hex {

    class Task {
    public:
        Task(const std::string& unlocalizedName, u64 maxValue);
        ~Task();

        void update(u64 currValue);
        void finish();

        [[nodiscard]]
        double getProgress() const;

        [[nodiscard]]
        const std::string& getName() const;

    private:
        std::string m_name;
        u64 m_maxValue, m_currValue;
    };

}