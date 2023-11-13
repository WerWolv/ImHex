#pragma once

#include <string>

namespace hex::prv {
    class Provider;
}

namespace hex::prv::undo {

    class Operation {
    public:
        virtual ~Operation() = default;

        virtual void undo(Provider *provider) = 0;
        virtual void redo(Provider *provider) = 0;

        [[nodiscard]] virtual std::string format() const = 0;
    };

}