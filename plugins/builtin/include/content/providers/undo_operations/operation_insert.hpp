#pragma once

#include <hex/providers/undo_redo/operations/operation.hpp>

#include <hex/helpers/fmt.hpp>
#include <hex/helpers/utils.hpp>

namespace hex::plugin::builtin::undo {

    class OperationInsert : public prv::undo::Operation {
    public:
        OperationInsert(u64 offset, u64 size) :
            m_offset(offset), m_size(size) { }

        void undo(prv::Provider *provider) override {
            provider->removeRaw(this->m_offset, this->m_size);
        }

        void redo(prv::Provider *provider) override {
            provider->insertRaw(this->m_offset, this->m_size);
        }

        [[nodiscard]] std::string format() const override {
            return hex::format("hex.builtin.undo_operation.insert"_lang, hex::toByteString(this->m_size), this->m_offset);
        }

        std::unique_ptr<Operation> clone() const override {
            return std::make_unique<OperationInsert>(*this);
        }

        [[nodiscard]] Region getRegion() const override {
            return { this->m_offset, this->m_size };
        }

    private:
        u64 m_offset;
        u64 m_size;
    };

}