#include <hex/data_processor/link.hpp>

#include <hex/helpers/shared_data.hpp>

namespace hex::dp {

    Link::Link(u32 from, u32 to) : m_id(SharedData::dataProcessorLinkIdCounter++), m_from(from), m_to(to) { }


}