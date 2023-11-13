#pragma once

#include <hex/providers/undo_redo/operations/operation.hpp>

#include <hex/helpers/fmt.hpp>
#include <hex/helpers/utils.hpp>

namespace hex::prv::undo {

    class OperationRemove : public Operation {
    public:
        OperationRemove(u64 offset, u64 size) :
            m_offset(offset), m_size(size) { }

        void undo(Provider *provider) override {
            provider->insert(this->m_offset, this->m_size);
        }

        void redo(Provider *provider) override {
            provider->remove(this->m_offset, this->m_size);
        }

        [[nodiscard]] std::string format() const override {
            return hex::format("Removed {} at 0x{:04x}", hex::toByteString(this->m_size), this->m_offset);
        }

        std::unique_ptr<Operation> clone() const override {
            return std::make_unique<OperationRemove>(*this);
        }

        [[nodiscard]] Region getRegion() const override {
            return { this->m_offset, this->m_size };
        }

    private:
        u64 m_offset;
        u64 m_size;
    };

}