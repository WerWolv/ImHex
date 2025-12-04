#pragma once

#include <hex/api/task_manager.hpp>
#include <hex/api/content_registry/hashes.hpp>
#include <hex/api/imhex_api/hex_editor.hpp>
#include <hex/providers/memory_provider.hpp>

#include <hex/ui/view.hpp>

namespace hex::plugin::hashes {

    class ViewHashes : public View::Window {
    public:
        explicit ViewHashes();
        ~ViewHashes() override;

        void drawContent() override;
        void drawHelpText() override;

        class Function {
        public:
            explicit Function(ContentRegistry::Hashes::Hash::Function hashFunction) : m_hashFunction(std::move(hashFunction)) { }

            void update(const Region &region, prv::Provider *provider) {
                m_region = { region, provider };
            }

            void update(std::vector<u8> data) {
                m_data = std::move(data);
            }

            std::vector<u8> get() {
                if (!m_task.isRunning()) {
                    if (m_region.has_value()) {
                        m_lastResult.clear();
                        m_task = TaskManager::createBackgroundTask("Updating hash", [this, region = m_region]() {
                            m_lastResult = m_hashFunction.get(region->getRegion(), region->getProvider());
                        });

                        m_region.reset();
                    } else if (!m_data.empty()) {
                        m_lastResult.clear();
                        m_task = TaskManager::createBackgroundTask("Updating hash", [this, data = std::move(m_data)]() {
                            prv::MemoryProvider provider({ data.begin(), data.end() });
                            m_lastResult = m_hashFunction.get(Region { 0x00, provider.getActualSize() }, &provider);
                        });

                        m_region.reset();
                        m_data = {};
                    }
                }

                return m_lastResult;
            }

            bool isCalculating() const {
                return m_task.isRunning();
            }

            const ContentRegistry::Hashes::Hash::Function& getFunction() const {
                return m_hashFunction;
            }

        private:
            std::vector<u8> m_data;
            std::optional<ImHexApi::HexEditor::ProviderRegion> m_region;
            ContentRegistry::Hashes::Hash::Function m_hashFunction;
            std::vector<u8> m_lastResult;
            TaskHolder m_task;
        };

    private:
        bool importHashes(prv::Provider *provider, const nlohmann::json &json);
        bool exportHashes(prv::Provider *provider, nlohmann::json &json);

        void drawAddHashPopup();

    private:
        ContentRegistry::Hashes::Hash *m_selectedHash = nullptr;
        std::string m_newHashName;

        PerProvider<std::list<Function>> m_hashFunctions;

    };

}
