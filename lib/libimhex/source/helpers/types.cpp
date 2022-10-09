#include <hex/helpers/types.hpp>

namespace hex {

    [[nodiscard]] bool Region::isWithin(const Region &other) const {
        if (*this == Invalid() || other == Invalid())
            return false;

        if (this->getStartAddress() >= other.getStartAddress() && this->getEndAddress() <= other.getEndAddress())
            return true;

        return false;
    }

    [[nodiscard]] bool Region::overlaps(const Region &other) const {
        if (*this == Invalid() || other == Invalid())
            return false;

        if (this->getEndAddress() >= other.getStartAddress() && this->getStartAddress() <= other.getEndAddress())
            return true;

        return false;
    }

    [[nodiscard]] u64 Region::getStartAddress() const {
        return this->address;
    }

    [[nodiscard]] u64 Region::getEndAddress() const {
        return this->address + this->size - 1;
    }

    [[nodiscard]] size_t Region::getSize() const {
        return this->size;
    }

    bool Region::operator==(const Region &other) const {
        return this->address == other.address && this->size == other.size;
    }

}