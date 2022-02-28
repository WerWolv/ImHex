#include <hex/helpers/project_file_handler.hpp>

#include <hex/api/imhex_api.hpp>

#include <fstream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace hex {

    fs::path ProjectFile::s_currProjectFilePath;
    bool ProjectFile::s_hasUnsavedChanged = false;

    fs::path ProjectFile::s_filePath;
    std::string ProjectFile::s_pattern;
    Patches ProjectFile::s_patches;
    std::list<ImHexApi::Bookmarks::Entry> ProjectFile::s_bookmarks;
    std::string ProjectFile::s_dataProcessorContent;

    void to_json(json &j, const ImHexApi::Bookmarks::Entry &b) {
        j = json {
            {"address",  b.region.address},
            { "size",    b.region.size   },
            { "name",    b.name.data()   },
            { "comment", b.comment.data()},
            { "locked",  b.locked        },
            { "color",   b.color         }
        };
    }

    void from_json(const json &j, ImHexApi::Bookmarks::Entry &b) {
        std::string name, comment;

        if (j.contains("address")) j.at("address").get_to(b.region.address);
        if (j.contains("size")) j.at("size").get_to(b.region.size);
        if (j.contains("name")) j.at("name").get_to(name);
        if (j.contains("comment")) j.at("comment").get_to(comment);
        if (j.contains("locked")) j.at("locked").get_to(b.locked);
        if (j.contains("color")) j.at("color").get_to(b.color);

        std::copy(name.begin(), name.end(), std::back_inserter(b.name));
        b.name.push_back('\0');
        std::copy(comment.begin(), comment.end(), std::back_inserter(b.comment));
        b.comment.push_back('\0');
    }


    bool ProjectFile::load(const fs::path &filePath) {
        ProjectFile::s_hasUnsavedChanged = false;

        json projectFileData;

        try {
            std::ifstream projectFile(filePath.c_str());
            projectFile >> projectFileData;

            ProjectFile::s_filePath             = fs::path(projectFileData["filePath"].get<std::string>());
            ProjectFile::s_pattern              = projectFileData["pattern"];
            ProjectFile::s_patches              = projectFileData["patches"].get<Patches>();
            ProjectFile::s_dataProcessorContent = projectFileData["dataProcessor"];

            ProjectFile::s_bookmarks.clear();
            for (auto &element : projectFileData["bookmarks"].items()) {
                ImHexApi::Bookmarks::Entry entry;
                from_json(element.value(), entry);
                ProjectFile::s_bookmarks.emplace_back(std::move(entry));
            }

        } catch (json::exception &e) {
            return false;
        } catch (std::ofstream::failure &e) {
            return false;
        }

        ProjectFile::s_currProjectFilePath = filePath;

        EventManager::post<EventProjectFileLoad>();

        return true;
    }

    bool ProjectFile::store(fs::path filePath) {
        EventManager::post<EventProjectFileStore>();

        json projectFileData;

        if (filePath.empty())
            filePath = ProjectFile::s_currProjectFilePath;

        try {
            projectFileData["filePath"]      = ProjectFile::s_filePath.string();
            projectFileData["pattern"]       = ProjectFile::s_pattern;
            projectFileData["patches"]       = ProjectFile::s_patches;
            projectFileData["dataProcessor"] = ProjectFile::s_dataProcessorContent;

            for (auto &bookmark : ProjectFile::s_bookmarks) {
                to_json(projectFileData["bookmarks"].emplace_back(), bookmark);
            }

            std::ofstream projectFile(filePath.c_str(), std::fstream::trunc);
            projectFile << projectFileData;
        } catch (json::exception &e) {
            return false;
        } catch (std::ifstream::failure &e) {
            return false;
        }

        ProjectFile::s_hasUnsavedChanged   = false;
        ProjectFile::s_currProjectFilePath = filePath;

        return true;
    }

}