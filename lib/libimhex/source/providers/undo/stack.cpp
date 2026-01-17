#include <hex/providers/undo_redo/stack.hpp>
#include <hex/providers/undo_redo/operations/operation_group.hpp>
#include <hex/api/events/events_interaction.hpp>

#include <hex/providers/provider.hpp>

#include <wolv/utils/guards.hpp>

#include <atomic>

namespace hex::prv::undo {

    namespace {

        std::recursive_mutex s_mutex;

    }

    Stack::Stack(Provider *provider) : m_provider(provider) {

    }

    std::recursive_mutex& Stack::getMutex() {
        return s_mutex;
    }


    void Stack::undo(u32 count) {
        std::lock_guard lock(s_mutex);

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
            EventDataChanged::post(m_provider);
        }
    }

    void Stack::redo(u32 count) {
        std::lock_guard lock(s_mutex);

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
            EventDataChanged::post(m_provider);
        }
    }

    void Stack::groupOperations(u32 count, const UnlocalizedString &unlocalizedName) {
        std::lock_guard lock(s_mutex);

        if (count <= 1)
            return;

        auto operation = std::make_unique<OperationGroup>(unlocalizedName);

        i64 startIndex = std::max<i64>(0, m_undoStack.size() - count);

        // Move operations from our stack to the group in the same order they were added
        for (u32 i = 0; i < count; i += 1) {
            i64 index = startIndex + i;

            if (index < 0 || u64(index) >= m_undoStack.size()) {
                break;
            }

            m_undoStack[index]->undo(m_provider);
            operation->addOperation(std::move(m_undoStack[index]));
        }

        // Remove the empty operations from the stack
        m_undoStack.resize(startIndex);
        this->add(std::move(operation));
    }

    void Stack::apply(const Stack &otherStack) {
        std::lock_guard lock(s_mutex);

        for (const auto &operation : otherStack.m_undoStack) {
            this->add(operation->clone());
        }
    }

    void Stack::reapply() {
        std::lock_guard lock(s_mutex);

        for (const auto &operation : m_undoStack) {
            operation->redo(m_provider);
            EventDataChanged::post(m_provider);
        }
    }




    bool Stack::add(std::unique_ptr<Operation> &&operation) {
        std::lock_guard lock(s_mutex);

        // Clear the redo stack
        m_redoStack.clear();

        // Insert the new operation at the end of the list
        m_undoStack.emplace_back(std::move(operation));

        // Do the operation
        this->getLastOperation()->redo(m_provider);

        EventDataChanged::post(m_provider);

        return true;
    }

    bool Stack::canUndo() const {
        std::lock_guard lock(s_mutex);

        return !m_undoStack.empty();
    }

    bool Stack::canRedo() const {
        std::lock_guard lock(s_mutex);

        return !m_redoStack.empty();
    }





}