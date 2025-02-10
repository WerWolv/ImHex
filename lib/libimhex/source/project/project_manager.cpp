#include <hex/helpers/auto_reset.hpp>
#include <hex/project/project_manager.hpp>

namespace hex::proj {

    static AutoReset<std::list<std::unique_ptr<Project>>> s_projects;
    static AutoReset<std::list<ProjectManager::ContentHandler>> s_contentHandlers;

    void ProjectManager::createProject(std::string name) {
        s_projects->emplace_back(std::make_unique<Project>(std::move(name)));
    }

    void ProjectManager::loadProject(const std::filesystem::path &path) {
        std::ignore = path;
    }

    void ProjectManager::removeProject(const Project &projectToClose) {
        std::erase_if(*s_projects, [&](const std::unique_ptr<Project> &project) {
            return project.get() == &projectToClose;
        });
    }


    const std::list<std::unique_ptr<Project>> &ProjectManager::getProjects() {
        return *s_projects;
    }

    const std::list<ProjectManager::ContentHandler> &ProjectManager::getContentHandlers() {
        return *s_contentHandlers;
    }

    void ProjectManager::registerContentHandler(ContentHandler handler) {
        s_contentHandlers->emplace_back(std::move(handler));
    }

    const ProjectManager::ContentHandler* ProjectManager::getContentHandler(const UnlocalizedString &typeName) {
        for (const auto &handler : getContentHandlers()) {
            if (handler.type == typeName) {
                return &handler;
            }
        }

        return nullptr;
    }

    void ProjectManager::storeContent(Content &content) {
        const auto *handler = getContentHandler(content.getType());
        if (handler != nullptr) {
            handler->store(content);
        }
    }

    void ProjectManager::loadContent(Content &content) {
        const auto *handler = getContentHandler(content.getType());

        if (handler != nullptr) {
            if (!handler->allowMultiple) {
                for (const auto &project : getProjects()) {
                    for (const auto &projectContent : project->getContents()) {
                        if (projectContent->isOpen() && projectContent->getType() == content.getType()) {
                            storeContent(*projectContent);
                            projectContent->setOpen(false);
                        }
                    }
                }
            }

            if (handler->type == content.getType()) {
                handler->load(content);
                content.setOpen(true);
            }
        }

    }

    Content* ProjectManager::getLoadedContent(const UnlocalizedString& type) {
        for (const auto &project : getProjects()) {
            for (const auto &projectContent : project->getContents()) {
                if (projectContent->isOpen() && projectContent->getType() == type) {
                    return projectContent.get();
                }
            }
        }

        return nullptr;
    }


}