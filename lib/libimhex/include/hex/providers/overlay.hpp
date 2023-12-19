#pragma once

#include <hex.hpp>

#include <vector>

namespace hex::prv {

    class Overlay {
    public:
        Overlay() = default;

        void setAddress(u64 address) { m_address = address; }
        [[nodiscard]] u64 getAddress() const { return m_address; }

        [[nodiscard]] u64 getSize() const { return m_data.size(); }
        [[nodiscard]] std::vector<u8> &getData() { return m_data; }

    private:
        u64 m_address = 0;
        std::vector<u8> m_data;
    };

}