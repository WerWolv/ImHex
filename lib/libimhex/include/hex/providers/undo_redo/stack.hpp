#pragma once

#include <hex.hpp>
#include <hex/api/localization_manager.hpp>
#include <hex/api/event_manager.hpp>

#include <hex/providers/undo_redo/operations/operation.hpp>

#include <map>
#include <memory>
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

        void groupOperations(u32 count, const UnlocalizedString &unlocalizedName);
        void apply(const Stack &otherStack);
        void reapply();

        [[nodiscard]] bool canUndo() const;
        [[nodiscard]] bool canRedo() const;

        template<std::derived_from<Operation> T>
        bool add(auto && ... args) {
            auto result = this->add(std::make_unique<T>(std::forward<decltype(args)>(args)...));
            EventDataChanged::post(m_provider);

            return result;
        }

        bool add(std::unique_ptr<Operation> &&operation);

        const std::vector<std::unique_ptr<Operation>> &getAppliedOperations() const {
            return m_undoStack;
        }

        const std::vector<std::unique_ptr<Operation>> &getUndoneOperations() const {
            return m_redoStack;
        }

        void reset() {
            m_undoStack.clear();
            m_redoStack.clear();
        }

    private:
        [[nodiscard]] Operation* getLastOperation() const {
            return m_undoStack.back().get();
        }

    private:
        std::vector<std::unique_ptr<Operation>> m_undoStack, m_redoStack;
        Provider *m_provider;
    };

}