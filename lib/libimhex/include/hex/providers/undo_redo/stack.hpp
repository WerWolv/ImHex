#pragma once

#include <hex.hpp>
#include <hex/providers/undo_redo/operations/operation.hpp>

#include <atomic>
#include <memory>
#include <mutex>
#include <vector>

namespace hex::prv {
    class Provider;
}

namespace hex::prv::undo {

    class Stack {
    public:
        explicit Stack(Provider *provider);

        void undo(u32 count = 1);
        void redo(u32 count = 1);

        void groupOperations(u32 count);

        [[nodiscard]] bool canUndo() const;
        [[nodiscard]] bool canRedo() const;

        template<std::derived_from<Operation> T>
        void add(auto && ... args) {
            this->add(std::make_unique<T>(std::forward<decltype(args)>(args)...));
        }

        void add(std::unique_ptr<Operation> &&operation);
    private:

        [[nodiscard]] Operation* getLastOperation() const {
            return this->m_undoStack.back().get();
        }

    private:
        std::vector<std::unique_ptr<Operation>> m_undoStack, m_redoStack;
        Provider *m_provider;

        std::atomic_bool m_locked;
        std::mutex m_mutex;
    };

}