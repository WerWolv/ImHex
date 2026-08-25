#include "internal.hpp"

#include <hex/api/content_registry/user_interface.hpp>
#include <hex/api/events/events_gui.hpp>
#include <hex/api/events/events_lifecycle.hpp>
#include <hex/api/events/events_provider.hpp>
#include <hex/api/project_manager.hpp>
#include <hex/helpers/logger.hpp>
#include <hex/providers/file_backed_provider_data.hpp>
#include <hex/providers/provider.hpp>

#include <fonts/vscode_icons.hpp>

namespace hex::plugin::builtin {

    void registerProjectHandlers() {
        using namespace project::impl;

        hex::ProjectManager::setProjectFunctions(load, store);
        EventImHexStartupFinished::subscribe([] {
            if (!ProjectManager::hasPath() && !ProjectManager::loadTemporaryProject())
                log::error("Failed to open the temporary project");
        });
        ContentRegistry::UserInterface::addSidebarItem("hex.builtin.sidebar.project.name"_unlocalized, ICON_VS_NOTEBOOK, drawProjectSidebar, [] {
            return ProjectManager::isFolderProject();
        });
        EventFileBackedProviderDataChanged::subscribe([](prv::Provider *provider, FileBackedProviderDataBase *data) {
            if (!ProjectManager::isFolderProject())
                return;

            materializeRegisteredData();
            if (provider != nullptr && data != nullptr && data->hasPendingData(provider))
                std::ignore = data->flush();
            std::ignore = persistProjectMetadata();
        });
        EventProviderOpened::subscribe([](prv::Provider *provider) {
            auto &projectState = state();
            const auto replacement = projectState.providerReplacements.find(provider);
            const bool isReplacement = replacement != projectState.providerReplacements.end();
            if (isReplacement) {
                provider->setID(replacement->second);
                projectState.providerReplacements.erase(replacement);
            }
            if (ProjectManager::isFolderProject() && !projectState.loadingProject) {
                if (ProjectManager::isTemporaryProject())
                    projectState.projectProviderIds.insert(provider->getID());
                projectState.closedProjectProviderIds.erase(provider->getID());
                snapshotProviderSettings(provider);
                if (isReplacement)
                    bindRegisteredData();
            }
            scheduleProjectMetadataSave();
        });
        EventProviderRemoving::subscribe([](prv::Provider *provider) {
            const auto &projectState = state();
            if (projectState.loadingProject || !ProjectManager::isFolderProject() ||
                !projectState.projectProviderIds.contains(provider->getID()) || projectState.providerOpenAttempts.contains(provider->getID()) ||
                isProviderReplacement(provider))
                return;

            snapshotProviderSettings(provider);
        });
        EventProviderClosed::subscribe([](prv::Provider *provider) {
            auto &projectState = state();
            if (isProviderReplacement(provider)) {
                projectState.providerReplacements.erase(provider);
                return;
            }
            if (projectState.loadingProject || !ProjectManager::isFolderProject() ||
                !projectState.projectProviderIds.contains(provider->getID()) || projectState.providerOpenAttempts.contains(provider->getID()))
                return;

            projectState.closedProjectProviderIds.insert(provider->getID());
            scheduleProjectMetadataSave();
        });
        EventProjectClosed::subscribe([] {
            auto &projectState = state();
            projectState.associations.clear();
            projectState.projectProviderIds.clear();
            projectState.closedProjectProviderIds.clear();
            projectState.providerOpenAttempts.clear();
            projectState.providerReplacements.clear();
            projectState.projectProviderSettings.clear();
            resetSidebarState();
        });
        EventProviderDirtied::subscribe([](prv::Provider *) {
            scheduleProjectMetadataSave();
        });
        EventWindowDeinitializing::subscribe([](GLFWwindow *) {
            if (ProjectManager::isFolderProject() && !state().skipShutdownStore)
                std::ignore = ProjectManager::store();
        });
    }

}
