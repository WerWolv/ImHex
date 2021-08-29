#include <hex/data_processor/attribute.hpp>

#include <hex/helpers/shared_data.hpp>

namespace hex::dp {

    Attribute::Attribute(IOType ioType, Type type, std::string_view unlocalizedName) : m_id(SharedData::dataProcessorAttrIdCounter++), m_ioType(ioType), m_type(type), m_unlocalizedName(unlocalizedName) {

    }

    Attribute::~Attribute() {
        for (auto &[linkId, attr] : this->getConnectedAttributes())
            attr->removeConnectedAttribute(linkId);
    }

}