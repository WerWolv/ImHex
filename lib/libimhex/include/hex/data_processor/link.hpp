#pragma once

#include <hex.hpp>

namespace hex::dp {

    class Link {
    public:
        Link(int from, int to);

        [[nodiscard]] int getId() const { return this->m_id; }
        void setId(int id) { this->m_id = id; }

        [[nodiscard]] int getFromId() const { return this->m_from; }
        [[nodiscard]] int getToId() const { return this->m_to; }

        static void setIdCounter(int id) {
            if (id > Link::s_idCounter)
                Link::s_idCounter = id;
        }

    private:
        int m_id;
        int m_from, m_to;

        static int s_idCounter;
    };

}