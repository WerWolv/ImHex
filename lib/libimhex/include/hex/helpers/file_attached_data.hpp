#pragma once

#include "default_paths.hpp"
#include "hex/api/project_manager.hpp"

#include <wolv/types/static_string.hpp>

namespace hex {

    template<wolv::type::StaticString Key, typename Type>
    class FileAttachedData {
    public:
        class Data {
        public:
            Data(std::fs::path path, nlohmann::json *json) : m_path(std::move(path)), m_json(json) {}

            [[nodiscard]] operator Type() {
                try {
                    return (*m_json)[getPathKey()].template get<Type>();
                } catch (const nlohmann::json::exception &e) {
                    // If value can't be loaded, default construct it
                    (*m_json)[getPathKey()] = Type();
                    return Type();
                }
            }

            Data& operator=(Type value) {
                (*m_json)[getPathKey()] = value;
                save();

                return *this;
            }

        private:
            std::string getPathKey() {
                const auto projectPath = ProjectManager::getPath();
                if (projectPath.empty()) {
                    return wolv::util::toUTF8String(m_path);
                } else {
                    return wolv::util::toUTF8String(std::fs::relative(m_path, projectPath));
                }
            }

            bool saveTo(const std::fs::path &directory) {
                wolv::io::fs::createDirectories(directory);

                const auto jsonString = m_json->dump(4);
                const auto result = wolv::io::File(directory / fmt::format("{}.json", Key.get()), wolv::io::File::Mode::Create).writeString(jsonString);

                return result == i64(jsonString.size());
            }

            void save() {
                const auto projectPath = ProjectManager::getPath();
                if (projectPath.empty()) {
                    for (const auto &path : paths::Variables.write()) {
                        if (saveTo(path))
                            break;
                    }
                } else {
                    saveTo(projectPath / ProjectManager::ProjectDirectory / "variables");
                }
            }

        private:
            std::fs::path m_path;
            nlohmann::json *m_json;
        };

        Data get(const std::fs::path &path) {
            loadDataIfNeeded();
            return Data(path, &m_data);
        }

    private:
        bool loadFrom(const std::fs::path &directory) {
            if (!wolv::io::fs::exists(directory))
                return false;

            auto file = wolv::io::File(directory / fmt::format("{}.json", Key.get()), wolv::io::File::Mode::Read);
            if (!file.isValid())
                return false;

            const auto jsonString = file.readString();
            if (jsonString.empty())
                return false;

            try {
                m_data = nlohmann::json::parse(jsonString);
            } catch (const std::exception &error) {
                log::debug("Failed to load file attached data {} from {}: {}", Key.get(), wolv::util::toUTF8String(directory), error.what());
                return false;
            }

            m_dataLoadPath = directory;
            return true;
        }

        void loadDataIfNeeded() {
            const auto projectPath = ProjectManager::getPath();
            if (projectPath.empty()) {
                bool shouldLoad = m_dataLoadPath.empty();
                for (const auto &path : paths::Variables.write()) {
                    if (shouldLoad)
                        break;

                    if (m_dataLoadPath == path)
                        break;

                    shouldLoad = true;
                }
                if (shouldLoad) {
                    for (const auto &path : paths::Variables.write()) {
                        if (loadFrom(path))
                            break;
                    }
                }
            } else {
                const auto directory = projectPath / ProjectManager::ProjectDirectory / "variables";
                if (m_dataLoadPath != directory)
                    loadFrom(directory);
            }
        }

    private:
        std::fs::path m_dataLoadPath;
        nlohmann::json m_data;
    };

}