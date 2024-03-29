#include <hex/providers/undo_redo/stack.hpp>
#include <hex/providers/undo_redo/operations/operation_group.hpp>

#include <hex/providers/provider.hpp>

#include <wolv/utils/guards.hpp>

#include <atomic>

namespace hex::prv::undo {

    namespace {

        std::atomic<bool> s_locked;
        std::mutex s_mutex;

    }

    Stack::Stack(Provider *provider) : m_provider(provider) {

    }


    void Stack::undo(u32 count) {
        std::scoped_lock lock(s_mutex);

        s_locked = true;
        ON_SCOPE_EXIT { s_locked = false; };

        // If there are no operations, we can't undo anything.
        if (m_undoStack.empty())
            return;

        for (u32 i = 0; i < count; i += 1) {
            // If we reached the start of the list, we can't undo anymore.
            if (!this->canUndo()) {
                return;
            }

            // Move last element from the undo stack to the redo stack
            m_redoStack.emplace_back(std::move(m_undoStack.back()));
            m_redoStack.back()->undo(m_provider);
            m_undoStack.pop_back();
        }
    }

    void Stack::redo(u32 count) {
        std::scoped_lock lock(s_mutex);

        s_locked = true;
        ON_SCOPE_EXIT { s_locked = false; };

        // If there are no operations, we can't redo anything.
        if (m_redoStack.empty())
            return;

        for (u32 i = 0; i < count; i += 1) {
            // If we reached the end of the list, we can't redo anymore.
            if (!this->canRedo()) {
                return;
            }

            // Move last element from the undo stack to the redo stack
            m_undoStack.emplace_back(std::move(m_redoStack.back()));
            m_undoStack.back()->redo(m_provider);
            m_redoStack.pop_back();
        }
    }

    void Stack::groupOperations(u32 count, const UnlocalizedString &unlocalizedName) {
        if (count <= 1)
            return;

        auto operation = std::make_unique<OperationGroup>(unlocalizedName);

        i64 startIndex = std::max<i64>(0, m_undoStack.size() - count);

        // Move operations from our stack to the group in the same order they were added
        for (u32 i = 0; i < count; i += 1) {
            i64 index = startIndex + i;

            m_undoStack[index]->undo(m_provider);
            operation->addOperation(std::move(m_undoStack[index]));
        }

        // Remove the empty operations from the stack
        m_undoStack.resize(startIndex);
        this->add(std::move(operation));
    }

    void Stack::apply(const Stack &otherStack) {
        for (const auto &operation : otherStack.m_undoStack) {
            this->add(operation->clone());
        }
    }

    void Stack::reapply() {
        for (const auto &operation : m_undoStack) {
            operation->redo(m_provider);
        }
    }




    bool Stack::add(std::unique_ptr<Operation> &&operation) {
        // If we're already inside of an undo/redo operation, ignore new operations being added
        if (s_locked)
            return false;

        s_locked = true;
        ON_SCOPE_EXIT { s_locked = false; };

        std::scoped_lock lock(s_mutex);

        // Clear the redo stack
        m_redoStack.clear();

        // Insert the new operation at the end of the list
        m_undoStack.emplace_back(std::move(operation));

        // Do the operation
        this->getLastOperation()->redo(m_provider);

        return true;
    }

    bool Stack::canUndo() const {
        return !m_undoStack.empty();
    }

    bool Stack::canRedo() const {
        return !m_redoStack.empty();
    }





}