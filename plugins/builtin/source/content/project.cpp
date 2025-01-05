#include <filesystem>


#include <wolv/utils/guards.hpp>
#include <wolv/utils/string.hpp>

#include <hex/api/imhex_api.hpp>
#include <hex/api/project_file_manager.hpp>
#include <hex/api/localization_manager.hpp>
#include <hex/api/achievement_manager.hpp>
#include <hex/api/content_registry.hpp>
#include <hex/api/event_manager.hpp>

#include <hex/providers/provider.hpp>
#include <hex/helpers/fmt.hpp>
#include <hex/helpers/logger.hpp>
#include <toasts/toast_notification.hpp>

namespace hex::plugin::builtin {

    constexpr static auto MetadataHeaderMagic   = "HEX";
    constexpr static auto MetadataPath          = "IMHEX_METADATA";

    bool load(const std::fs::path &filePath) {
        if (!wolv::io::fs::exists(filePath) || !wolv::io::fs::isRegularFile(filePath)) {
            ui::ToastError::open(hex::format("hex.builtin.popup.error.project.load"_lang,
                hex::format("hex.builtin.popup.error.project.load.file_not_found"_lang,
                    wolv::util::toUTF8String(filePath)
            )));

            return false;
        }

        Tar tar(filePath, Tar::Mode::Read);
        if (!tar.isValid()) {
            ui::ToastError::open(hex::format("hex.builtin.popup.error.project.load"_lang,
                hex::format("hex.builtin.popup.error.project.load.invalid_tar"_lang,
                    tar.getOpenErrorString()
            )));

            return false;
        }

        if (!tar.contains(MetadataPath)) {
            ui::ToastError::open(hex::format("hex.builtin.popup.error.project.load"_lang,
                hex::format("hex.builtin.popup.error.project.load.invalid_magic"_lang)
            ));

            return false;
        }

        {
            const auto metadataContent = tar.readVector(MetadataPath);

            if (!std::string(metadataContent.begin(), metadataContent.end()).starts_with(MetadataHeaderMagic)) {
                ui::ToastError::open(hex::format("hex.builtin.popup.error.project.load"_lang,
                    hex::format("hex.builtin.popup.error.project.load.invalid_magic"_lang)
                ));

                return false;
            }
        }

        for (const auto &provider : ImHexApi::Provider::getProviders()) {
            ImHexApi::Provider::remove(provider);
        }

        auto originalPath = ProjectFile::getPath();
        ProjectFile::setPath(filePath);
        auto resetPath = SCOPE_GUARD {
            ProjectFile::setPath(originalPath);
        };

        for (const auto &handler : ProjectFile::getHandlers()) {
            bool result = true;
            // Handlers are supposed to show the error/warning popup to the user themselves, so we don't show one here
            try {
                if (!handler.load(handler.basePath, tar)) {
                    log::warn("Project file handler for {} failed to load {}", filePath.string(), handler.basePath.string());
                    result = false;
                }
            } catch (std::exception &e) {
                log::warn("Project file handler for {} failed to load {}: {}", filePath.string(), handler.basePath.string(), e.what());
                result = false;
            }

            if (!result && handler.required) {
                return false;
            }
        }

        for (const auto &provider : ImHexApi::Provider::getProviders()) {
            const auto basePath = std::fs::path(std::to_string(provider->getID()));
            for (const auto &handler: ProjectFile::getProviderHandlers()) {
                bool result = true;
                // Handlers are supposed to show the error/warning popup to the user themselves, so we don't show one here
                try {
                    if (!handler.load(provider, basePath / handler.basePath, tar))
                        result = false;
                } catch (std::exception &e) {
                    log::info("{}", e.what());
                    result = false;
                }

                if (!result && handler.required) {
                    return false;
                }
            }
        }

        resetPath.release();
        EventProjectOpened::post();
        RequestUpdateWindowTitle::post();

        return true;
    }
    
    bool store(std::optional<std::fs::path> filePath = std::nullopt, bool updateLocation = true) {
        auto originalPath = ProjectFile::getPath();

        if (!filePath.has_value())
            filePath = originalPath;

        ProjectFile::setPath(filePath.value());
        auto resetPath = SCOPE_GUARD {
            ProjectFile::setPath(originalPath);
        };

        Tar tar(*filePath, Tar::Mode::Create);
        if (!tar.isValid())
            return false;

        bool result = true;
        for (const auto &handler : ProjectFile::getHandlers()) {
            try {
                if (!handler.store(handler.basePath, tar) && handler.required)
                    result = false;
            } catch (std::exception &e) {
                log::info("{}", e.what());

                if (handler.required)
                    result = false;
            }
        }
        for (const auto &provider : ImHexApi::Provider::getProviders()) {
            const auto basePath = std::fs::path(std::to_string(provider->getID()));
            for (const auto &handler: ProjectFile::getProviderHandlers()) {
                try {
                    if (!handler.store(provider, basePath / handler.basePath, tar) && handler.required)
                        result = false;
                } catch (std::exception &e) {
                    log::info("{}", e.what());

                    if (handler.required)
                        result = false;
                }
            }
        }

        {
            const auto metadataContent = hex::format("{}\n{}", MetadataHeaderMagic, ImHexApi::System::getImHexVersion().get(false));
            tar.writeString(MetadataPath, metadataContent);
        }

        ImHexApi::Provider::resetDirty();

        // If saveLocation is false, reset the project path (do not release the lock)
        if (updateLocation) {
            resetPath.release();

            // Request, as this puts us into a project state
            RequestUpdateWindowTitle::post();
        }

        AchievementManager::unlockAchievement("hex.builtin.achievement.starting_out", "hex.builtin.achievement.starting_out.save_project.name");

        return result;
    }

    void registerProjectHandlers() {
        hex::ProjectFile::setProjectFunctions(load, store);
    }
}
