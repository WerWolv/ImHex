#pragma once

#include <hex.hpp>

#include <vector>
#include <string>
#include <atomic>

#include <hex/api/task.hpp>

#include <imgui.h>
#include <hex/ui/imgui_imhex_extensions.h>

namespace hex::ui {

    template<typename T>
    class SearchableWidget {
    public:
        SearchableWidget(const std::function<bool(const std::string&, const T&)> &comparator) : m_comparator(comparator) {

        }

        const std::vector<const T*> &draw(const auto &entries) {
            if (this->m_filteredEntries.empty() && this->m_searchBuffer.empty()) {
                for (auto &entry : entries)
                    this->m_filteredEntries.push_back(&entry);
            }

            if (ImGui::InputText("##search", this->m_searchBuffer)) {
                this->m_pendingUpdate = true;
            }

            if (this->m_pendingUpdate && !this->m_updateTask.isRunning()) {
                this->m_pendingUpdate = false;
                this->m_filteredEntries.clear();
                this->m_filteredEntries.reserve(entries.size());

                this->m_updateTask = TaskManager::createBackgroundTask("Searching", [this, &entries, searchBuffer = this->m_searchBuffer](Task&) {
                    for (auto &entry : entries) {
                        if (searchBuffer.empty() || this->m_comparator(searchBuffer, entry))
                            this->m_filteredEntries.push_back(&entry);
                    }
                });

            }

            return this->m_filteredEntries;
        }

        void reset() {
            this->m_filteredEntries.clear();
        }
    private:
        std::atomic<bool> m_pendingUpdate = false;
        TaskHolder m_updateTask;

        std::string m_searchBuffer;
        std::vector<const T*> m_filteredEntries;
        std::function<bool(const std::string&, const T&)> m_comparator;
    };

}