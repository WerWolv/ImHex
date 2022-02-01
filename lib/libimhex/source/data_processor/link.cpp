#include <hex/data_processor/link.hpp>


namespace hex::dp {

    u32 Link::s_idCounter = 1;

    Link::Link(u32 from, u32 to) : m_id(Link::s_idCounter++), m_from(from), m_to(to) { }


}