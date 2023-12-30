#pragma once

#include <hex/providers/undo_redo/operations/operation.hpp>

#include <hex/api/localization_manager.hpp>
#include <hex/helpers/fmt.hpp>
#include <hex/helpers/utils.hpp>

namespace hex::prv::undo {

    class OperationGroup : public Operation {
    public:
        explicit OperationGroup(UnlocalizedString unlocalizedName) : m_unlocalizedName(std::move(unlocalizedName)) {}

        OperationGroup(const OperationGroup &other) {
            for (const auto &operation : other.m_operations)
                m_operations.emplace_back(operation->clone());
        }

        void undo(Provider *provider) override {
            for (auto &operation : m_operations)
                operation->undo(provider);
        }

        void redo(Provider *provider) override {
            for (auto &operation : m_operations)
                operation->redo(provider);
        }

        void addOperation(std::unique_ptr<Operation> &&newOperation) {
            auto newRegion = newOperation->getRegion();
            if (newRegion.getStartAddress() < m_startAddress)
                m_startAddress = newRegion.getStartAddress();
            if (newRegion.getEndAddress() > m_endAddress)
                m_endAddress = newRegion.getEndAddress();

            if (m_formattedContent.size() <= 10)
                m_formattedContent.emplace_back(newOperation->format());
            else
                m_formattedContent.back() = hex::format("[{}x] ...", (m_operations.size() - 10) + 1);

            m_operations.emplace_back(std::move(newOperation));
        }

        [[nodiscard]] std::string format() const override {
            return hex::format("{}", Lang(m_unlocalizedName));
        }

        [[nodiscard]] Region getRegion() const override {
            return Region { m_startAddress, (m_endAddress - m_startAddress) + 1 };
        }

        std::unique_ptr<Operation> clone() const override {
            return std::make_unique<OperationGroup>(*this);
        }

        std::vector<std::string> formatContent() const override {
            return m_formattedContent;
        }

    private:
        UnlocalizedString m_unlocalizedName;
        std::vector<std::unique_ptr<Operation>> m_operations;

        u64 m_startAddress = std::numeric_limits<u64>::max();
        u64 m_endAddress   = std::numeric_limits<u64>::min();
        std::vector<std::string> m_formattedContent;
    };

}