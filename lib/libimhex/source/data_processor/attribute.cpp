#include <hex/data_processor/attribute.hpp>


namespace hex::dp {

    int Attribute::s_idCounter = 1;


    Attribute::Attribute(IOType ioType, Type type, UnlocalizedString unlocalizedName) : m_id(s_idCounter++), m_ioType(ioType), m_type(type), m_unlocalizedName(std::move(unlocalizedName)) {
    }

    Attribute::~Attribute() {
        for (auto &[linkId, attr] : this->getConnectedAttributes())
            attr->removeConnectedAttribute(linkId);
    }

    void Attribute::setIdCounter(int id) {
        if (id > s_idCounter)
            s_idCounter = id;
    }

}