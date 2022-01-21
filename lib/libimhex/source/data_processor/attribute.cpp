#include <hex/data_processor/attribute.hpp>


namespace hex::dp {

    u32 Attribute::s_idCounter = 1;

    Attribute::Attribute(IOType ioType, Type type, std::string unlocalizedName) : m_id(Attribute::s_idCounter++), m_ioType(ioType), m_type(type), m_unlocalizedName(std::move(unlocalizedName)) {
        this->m_allowsImmediate = ioType == Attribute::IOType::In &&
            (type == Attribute::Type::Integer || type == Attribute::Type::Float);
    }

    Attribute::~Attribute() {
        for (auto &[linkId, attr] : this->getConnectedAttributes())
            attr->removeConnectedAttribute(linkId);
    }

}
