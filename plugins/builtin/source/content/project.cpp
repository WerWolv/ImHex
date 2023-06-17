#include <filesystem>

#include <wolv/utils/guards.hpp>
#include <wolv/utils/string.hpp>

#include <hex/api/project_file_manager.hpp>
#include <hex/api/localization.hpp>
#include <hex/providers/provider.hpp>
#include <hex/helpers/fmt.hpp>

#include <content/helpers/notification.hpp>

namespace hex::plugin::builtin {

    constexpr static auto MetadataHeaderMagic = "HEX";
    constexpr static auto MetadataPath = "IMHEX_METADATA";

    bool load(const std::fs::path &filePath) { 
        auto originalPath = ProjectFile::getPath();

        ProjectFile::setPath(filePath);
        auto resetPath = SCOPE_GUARD {
            ProjectFile::setPath(originalPath);
        };

        if (!wolv::io::fs::exists(filePath) || !wolv::io::fs::isRegularFile(filePath)) {
            showError(hex::format("hex.builtin.popup.error.project.load"_lang,
                hex::format("hex.builtin.popup.error.project.load.file_not_found"_lang,
                    wolv::util::toUTF8String(filePath)
            )));
            return false;
        }

        Tar tar(filePath, Tar::Mode::Read);
        if (!tar.isValid()) {
            showError(hex::format("hex.builtin.popup.error.project.load"_lang,
                hex::format("hex.builtin.popup.error.project.load.invalid_tar"_lang,
                    tar.getOpenErrorString()
            )));
            return false;
        }

        if (!tar.contains(MetadataPath)) {
            showError(hex::format("hex.builtin.popup.error.project.load"_lang,
                hex::format("hex.builtin.popup.error.project.load.invalid_magic"_lang)
            ));
            return false;
        }

        {
            const auto metadataContent = tar.readVector(MetadataPath);

            if (!std::string(metadataContent.begin(), metadataContent.end()).starts_with(MetadataHeaderMagic)) {
                showError(hex::format("hex.builtin.popup.error.project.load"_lang,
                    hex::format("hex.builtin.popup.error.project.load.invalid_magic"_lang)
                ));
                return false;
            }
        }

        auto providers = auto(ImHexApi::Provider::getProviders());
        for (const auto &provider : providers) {
            ImHexApi::Provider::remove(provider);
        }

        // TODO there is a bug here with the var result ?
        bool result = true;
        for (const auto &handler : ProjectFile::getHandlers()) {
            // handlers are supposed to show the error/warning popup to the user themselves, so we don't show one here 
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
                // handlers are supposed to show the error/warning popup to the user themselves, so we don't show one here
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
        EventManager::post<EventProjectOpened>();
        EventManager::post<RequestUpdateWindowTitle>();

        return true;
    }
    
    bool store(std::optional<std::fs::path> filePath = std::nullopt) {
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
            const auto metadataContent = hex::format("{}\n{}", MetadataHeaderMagic, IMHEX_VERSION);
            tar.writeString(MetadataPath, metadataContent);
        }

        ImHexApi::Provider::resetDirty();
        resetPath.release();

        return result;
    }

    void registerProjectHandlers() {
        hex::setProjectFunctions(load, store);    
    }
}