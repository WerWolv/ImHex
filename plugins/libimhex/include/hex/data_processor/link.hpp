#pragma once

namespace hex::dp {

    class Link {
    public:
        Link(u32 from, u32 to) : m_id(SharedData::dataProcessorLinkIdCounter++), m_from(from), m_to(to) { }

        [[nodiscard]] u32 getID()     const { return this->m_id;   }
        void setID(u32 id) { this->m_id = id; }

        [[nodiscard]] u32 getFromID() const { return this->m_from; }
        [[nodiscard]] u32 getToID()   const { return this->m_to;   }

    private:
        u32 m_id;
        u32 m_from, m_to;
    };

}