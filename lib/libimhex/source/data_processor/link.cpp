#include <hex/data_processor/link.hpp>


namespace hex::dp {

    int Link::s_idCounter = 1;

    Link::Link(int from, int to) : m_id(Link::s_idCounter++), m_from(from), m_to(to) { }


}