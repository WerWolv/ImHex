#pragma once

namespace hex::dp {

    class Link {
    public:
        Link(int from, int to);

        [[nodiscard]] int getId() const { return m_id; }
        void setId(int id) { m_id = id; }

        [[nodiscard]] int getFromId() const { return m_from; }
        [[nodiscard]] int getToId() const { return m_to; }

        static void setIdCounter(int id);

    private:
        int m_id;
        int m_from, m_to;

        static int s_idCounter;
    };

}