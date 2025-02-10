#pragma once

#include <hex.hpp>
#include <hex/project/project.hpp>

#include <list>
#include <string>
#include <filesystem>
#include <functional>

namespace hex::proj {

    class ProjectManager {
        ProjectManager() = default;

    public:
        static void createProject(std::string name);
        static void loadProject(const std::filesystem::path &path);
        static void removeProject(const Project &project);

        using LoadFunction = std::function<void(const Content&)>;
        using StoreFunction = std::function<void(Content&)>;

        struct ContentHandler {
            UnlocalizedString type;
            bool allowMultiple = false;
            LoadFunction load;
            StoreFunction store;
        };

        static void registerContentHandler(ContentHandler handler);
        static const std::list<ContentHandler>& getContentHandlers();
        static const ContentHandler* getContentHandler(const UnlocalizedString &typeName);

        static const std::list<std::unique_ptr<Project>> &getProjects();

        static void storeContent(Content &content);
        static void loadContent(Content &content);

        static Content* getLoadedContent(const UnlocalizedString &type);
    };

}
