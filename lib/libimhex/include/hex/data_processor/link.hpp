#pragma once

#include <hex.hpp>

namespace hex::dp {

    class Link {
    public:
        Link(u32 from, u32 to);

        [[nodiscard]] u32 getId() const { return this->m_id; }
        void setID(u32 id) { this->m_id = id; }

        [[nodiscard]] u32 getFromId() const { return this->m_from; }
        [[nodiscard]] u32 getToId() const { return this->m_to; }

        static void setIdCounter(u32 id) {
            if (id > Link::s_idCounter)
                Link::s_idCounter = id;
        }

    private:
        u32 m_id;
        u32 m_from, m_to;

        static u32 s_idCounter;
    };

}