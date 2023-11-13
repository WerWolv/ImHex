#pragma once

#include <hex/providers/undo_redo/operations/operation.hpp>

#include <hex/helpers/fmt.hpp>
#include <hex/helpers/utils.hpp>

namespace hex::prv::undo {

    class OperationGroup : public Operation {
    public:
        OperationGroup() = default;

        OperationGroup(const OperationGroup &other) {
            for (const auto &operation : other.m_operations)
                this->m_operations.emplace_back(operation->clone());
        }

        void undo(Provider *provider) override {
            for (auto &operation : this->m_operations)
                operation->undo(provider);
        }

        void redo(Provider *provider) override {
            for (auto &operation : this->m_operations)
                operation->redo(provider);
        }

        void addOperation(std::unique_ptr<Operation> &&operation) {
            this->m_operations.emplace_back(std::move(operation));
        }

        [[nodiscard]] std::string format() const override {
            return hex::format("Group of {} operations", this->m_operations.size());
        }

        [[nodiscard]] Region getRegion() const override {
            u64 minAddress = std::numeric_limits<u64>::max();
            u64 maxAddress = std::numeric_limits<u64>::min();

            for (const auto &operation : this->m_operations) {
                const auto [address, size] = operation->getRegion();

                minAddress = std::min(minAddress, address);
                maxAddress = std::max(maxAddress, address + size);
            }

            return { minAddress, (maxAddress - minAddress) + 1 };
        }

        std::unique_ptr<Operation> clone() const override {
            return std::make_unique<OperationGroup>(*this);
        }

    private:
        std::vector<std::unique_ptr<Operation>> m_operations;
    };

}