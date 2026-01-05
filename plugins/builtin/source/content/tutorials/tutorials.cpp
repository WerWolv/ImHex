#include <hex/api/tutorial_manager.hpp>
#include <ui/markdown.hpp>

namespace hex::plugin::builtin {

    void registerIntroductionTutorial();

    void registerTutorials() {
        TutorialManager::setRenderer([](const std::string &message) {
            return [markdown = std::make_shared<ui::Markdown>(message)] {
                markdown->draw();
            };
        });

        registerIntroductionTutorial();
    }

}
