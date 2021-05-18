#include "helpers/project_file_handler.hpp"

#include <fstream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace hex {

    void to_json(json& j, const ImHexApi::Bookmarks::Entry& b) {
        j = json{ { "address", b.region.address }, { "size", b.region.size }, { "name", b.name.data() }, { "comment", b.comment.data() }, { "locked", b.locked } };
    }

    void from_json(const json& j, ImHexApi::Bookmarks::Entry& b) {
        std::string name, comment;

        j.at("address").get_to(b.region.address);
        j.at("size").get_to(b.region.size);
        j.at("name").get_to(name);
        j.at("comment").get_to(comment);
        j.at("locked").get_to(b.locked);

        std::copy(name.begin(), name.end(), std::back_inserter(b.name));
        b.name.push_back('\0');
        std::copy(comment.begin(), comment.end(), std::back_inserter(b.comment));
        b.comment.push_back('\0');
    }


    bool ProjectFile::load(std::string_view filePath) {
        ProjectFile::s_hasUnsavedChanged = false;

        json projectFileData;

        try {
            std::ifstream projectFile(filePath.data());
            projectFile >> projectFileData;

            ProjectFile::s_filePath             = projectFileData["filePath"];
            ProjectFile::s_pattern              = projectFileData["pattern"];
            ProjectFile::s_patches              = projectFileData["patches"].get<Patches>();
            ProjectFile::s_dataProcessorContent = projectFileData["dataProcessor"];

            for (auto &element : projectFileData["bookmarks"].items()) {
                ProjectFile::s_bookmarks.push_back(element.value().get<ImHexApi::Bookmarks::Entry>());
            }

        } catch (json::exception &e) {
            return false;
        } catch (std::ofstream::failure &e) {
            return false;
        }

        ProjectFile::s_currProjectFilePath = filePath;

        return true;
    }

    bool ProjectFile::store(std::string_view filePath) {
        ProjectFile::s_hasUnsavedChanged = false;

        json projectFileData;

        if (filePath.empty())
            filePath = ProjectFile::s_currProjectFilePath;

        try {
            projectFileData["filePath"]         = ProjectFile::s_filePath;
            projectFileData["pattern"]          = ProjectFile::s_pattern;
            projectFileData["patches"]          = ProjectFile::s_patches;
            projectFileData["dataProcessor"]    = ProjectFile::s_dataProcessorContent;

            for (auto &bookmark : ProjectFile::s_bookmarks) {
                projectFileData["bookmarks"].push_back(bookmark);
            }

            std::ofstream projectFile(filePath.data(), std::fstream::trunc);
            projectFile << projectFileData;
        } catch (json::exception &e) {
            return false;
        } catch (std::ifstream::failure &e) {
            return false;
        }

        ProjectFile::s_currProjectFilePath = filePath;

        return true;
    }

}