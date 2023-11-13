#pragma once

#include <hex/providers/undo_redo/operations/operation.hpp>

#include <hex/helpers/fmt.hpp>
#include <hex/helpers/utils.hpp>

namespace hex::prv::undo {

    class OperationInsert : public Operation {
    public:
        OperationInsert(u64 offset, u64 size) :
            m_offset(offset), m_size(size) { }

        void undo(Provider *provider) override {
            provider->remove(this->m_offset, this->m_size);
        }

        void redo(Provider *provider) override {
            provider->insert(this->m_offset, this->m_size);
        }

        [[nodiscard]] std::string format() const override {
            return hex::format("Inserted {} at 0x{:04x}", this->m_offset, hex::toByteString(this->m_size));
        }

    private:
        u64 m_offset;
        u64 m_size;
    };

}