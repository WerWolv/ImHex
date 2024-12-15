#pragma once

#include <hex/helpers/crypto.hpp>
#include <hex/providers/undo_redo/operations/operation.hpp>

#include <hex/helpers/fmt.hpp>
#include <hex/helpers/utils.hpp>

#include <fonts/vscode_icons.hpp>

namespace hex::plugin::builtin::undo {

    class OperationWrite : public prv::undo::Operation {
    public:
        OperationWrite(u64 offset, u64 size, const u8 *oldData, const u8 *newData) :
            m_offset(offset),
            m_oldData(oldData, oldData + size),
            m_newData(newData, newData + size) { }

        void undo(prv::Provider *provider) override {
            provider->writeRaw(m_offset, m_oldData.data(), m_oldData.size());
        }

        void redo(prv::Provider *provider) override {
            provider->writeRaw(m_offset, m_newData.data(), m_newData.size());
        }

        [[nodiscard]] std::string format() const override {
            return hex::format("hex.builtin.undo_operation.write"_lang, hex::toByteString(m_newData.size()), m_offset);
        }

        std::vector<std::string> formatContent() const override {
            return {
                hex::format("{} {} {}", hex::crypt::encode16(m_oldData), ICON_VS_ARROW_RIGHT, hex::crypt::encode16(m_newData)),
            };
        }

        std::unique_ptr<Operation> clone() const override {
            return std::make_unique<OperationWrite>(*this);
        }

        [[nodiscard]] Region getRegion() const override {
            return { m_offset, m_oldData.size() };
        }

    private:
        u64 m_offset;
        std::vector<u8> m_oldData, m_newData;
    };

}
