#pragma once

#include <hex.hpp>

#include <hex/api/task_manager.hpp>
#include <hex/api/localization_manager.hpp>

#include <nlohmann/json_fwd.hpp>

#include <atomic>
#include <functional>
#include <memory>
#include <vector>

EXPORT_MODULE namespace hex {

    #if !defined(HEX_MODULE_EXPORT)
        namespace prv { class Provider; }
    #endif

    /* Data Information Registry. Allows adding new analyzers to the data information view */
    namespace ContentRegistry::DataInformation {

        class InformationSection {
        public:
            InformationSection(const UnlocalizedString &unlocalizedName, const UnlocalizedString &unlocalizedDescription = "", bool hasSettings = false)
                : m_unlocalizedName(unlocalizedName), m_unlocalizedDescription(unlocalizedDescription),
                  m_hasSettings(hasSettings) { }
            virtual ~InformationSection() = default;

            [[nodiscard]] const UnlocalizedString& getUnlocalizedName() const { return m_unlocalizedName; }
            [[nodiscard]] const UnlocalizedString& getUnlocalizedDescription() const { return m_unlocalizedDescription; }

            virtual void process(Task &task, prv::Provider *provider, Region region) = 0;
            virtual void reset() = 0;

            virtual void drawSettings() { }
            virtual void drawContent() = 0;

            [[nodiscard]] bool isValid() const { return m_valid; }
            void markValid(bool valid = true) { m_valid = valid; }

            [[nodiscard]] bool isEnabled() const { return m_enabled; }
            void setEnabled(bool enabled) { m_enabled = enabled; }

            [[nodiscard]] bool isAnalyzing() const { return m_analyzing; }
            void setAnalyzing(bool analyzing) { m_analyzing = analyzing; }

            virtual void load(const nlohmann::json &data);
            [[nodiscard]] virtual nlohmann::json store();

            [[nodiscard]] bool hasSettings() const { return m_hasSettings; }

        private:
            UnlocalizedString m_unlocalizedName, m_unlocalizedDescription;
            bool m_hasSettings;

            std::atomic<bool> m_analyzing = false;
            std::atomic<bool> m_valid     = false;
            std::atomic<bool> m_enabled   = true;
        };

        namespace impl {

            using CreateCallback = std::function<std::unique_ptr<InformationSection>()>;

            const std::vector<CreateCallback>& getInformationSectionConstructors();
            void addInformationSectionCreator(const CreateCallback &callback);

        }

        template<typename T>
        void addInformationSection(auto && ...args) {
            impl::addInformationSectionCreator([args...] {
                return std::make_unique<T>(std::forward<decltype(args)>(args)...);
            });
        }

    }

}