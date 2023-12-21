#pragma once

#include <hex.hpp>

#include <vector>
#include <string>
#include <atomic>

#include <hex/api/task_manager.hpp>

#include <imgui.h>
#include <hex/ui/imgui_imhex_extensions.h>

namespace hex::ui {

    template<typename T>
    class SearchableWidget {
    public:
        SearchableWidget(const std::function<bool(const std::string&, const T&)> &comparator) : m_comparator(comparator) {

        }

        const std::vector<const T*> &draw(const auto &entries) {
            if (m_filteredEntries.empty() && m_searchBuffer.empty()) {
                for (auto &entry : entries)
                    m_filteredEntries.push_back(&entry);
            }

            if (ImGui::InputText("##search", m_searchBuffer)) {
                m_pendingUpdate = true;
            }

            if (m_pendingUpdate && !m_updateTask.isRunning()) {
                m_pendingUpdate = false;
                m_filteredEntries.clear();
                m_filteredEntries.reserve(entries.size());

                m_updateTask = TaskManager::createBackgroundTask("Searching", [this, &entries, searchBuffer = m_searchBuffer](Task&) {
                    for (auto &entry : entries) {
                        if (searchBuffer.empty() || m_comparator(searchBuffer, entry))
                            m_filteredEntries.push_back(&entry);
                    }
                });

            }

            return m_filteredEntries;
        }

        void reset() {
            m_filteredEntries.clear();
        }
    private:
        std::atomic<bool> m_pendingUpdate = false;
        TaskHolder m_updateTask;

        std::string m_searchBuffer;
        std::vector<const T*> m_filteredEntries;
        std::function<bool(const std::string&, const T&)> m_comparator;
    };

}