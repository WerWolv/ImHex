#include <hex/project/project.hpp>
#include <hex/project/project_manager.hpp>

namespace hex::proj {

    Project::~Project() {
        for (auto &content : m_contents) {
            ProjectManager::storeContent(*content);
        }
    }


    void Project::addContent(UnlocalizedString type) {
        const auto &content = m_contents.emplace_back(
            std::make_unique<Content>(
                type,
                fmt::format("Unnamed {}", Lang(type))
            )
        );

        const auto loadedContent = ProjectManager::getLoadedContent(type);
        ProjectManager::storeContent(loadedContent == nullptr ? *content : *loadedContent);

        ProjectManager::loadContent(*content);
    }


}
