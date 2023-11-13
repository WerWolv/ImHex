#pragma once

#include <hex/providers/undo_redo/operations/operation.hpp>

#include <hex/helpers/fmt.hpp>
#include <hex/helpers/utils.hpp>

namespace hex::prv::undo {

    class OperationGroup : public Operation {
    public:
        OperationGroup() = default;

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

    private:
        std::vector<std::unique_ptr<Operation>> m_operations;
    };

}