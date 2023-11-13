#pragma once

#include <string>
#include <hex/helpers/concepts.hpp>

namespace hex::prv {
    class Provider;
}

namespace hex::prv::undo {

    class Operation : public ICloneable<Operation> {
    public:
        virtual ~Operation() = default;

        virtual void undo(Provider *provider) = 0;
        virtual void redo(Provider *provider) = 0;

        [[nodiscard]] virtual std::string format() const = 0;
        [[nodiscard]] virtual Region getRegion() const = 0;
    };

}