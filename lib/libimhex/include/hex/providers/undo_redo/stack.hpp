#pragma once

#include <hex.hpp>
#include <hex/providers/undo_redo/operations/operation.hpp>

#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <vector>

namespace hex::prv {
    class Provider;
}

namespace hex::prv::undo {

    using Patches = std::map<u64, u8>;

    class Stack {
    public:
        explicit Stack(Provider *provider);

        void undo(u32 count = 1);
        void redo(u32 count = 1);

        void groupOperations(u32 count);
        void apply(const Stack &otherStack);

        [[nodiscard]] bool canUndo() const;
        [[nodiscard]] bool canRedo() const;

        template<std::derived_from<Operation> T>
        bool add(auto && ... args) {
            return this->add(std::make_unique<T>(std::forward<decltype(args)>(args)...));
        }

        bool add(std::unique_ptr<Operation> &&operation);

        const std::vector<std::unique_ptr<Operation>> &getOperations() const {
            return this->m_undoStack;
        }
    private:
        [[nodiscard]] Operation* getLastOperation() const {
            return this->m_undoStack.back().get();
        }

    private:
        std::vector<std::unique_ptr<Operation>> m_undoStack, m_redoStack;
        Provider *m_provider;
    };

}