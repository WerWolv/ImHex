#pragma once

#include <hex/providers/undo_redo/operations/operation.hpp>

#include <hex/helpers/fmt.hpp>
#include <hex/helpers/utils.hpp>

namespace hex::prv::undo {

    class OperationWrite : public Operation {
    public:
        OperationWrite(u64 offset, u64 size, const u8 *oldData, const u8 *newData) :
            m_offset(offset),
            m_oldData(oldData, oldData + size),
            m_newData(newData, newData + size) { }

        void undo(Provider *provider) override {
            provider->writeRaw(this->m_offset, this->m_oldData.data(), this->m_oldData.size());
        }

        void redo(Provider *provider) override {
            provider->writeRaw(this->m_offset, this->m_newData.data(), this->m_newData.size());
        }

        [[nodiscard]] std::string format() const override {
            return hex::format("Written {} at 0x{:04X}", hex::toByteString(this->m_newData.size()), this->m_offset);
        }

    private:
        u64 m_offset;
        std::vector<u8> m_oldData, m_newData;
    };

}
