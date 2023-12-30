#include <content/providers/memory_file_provider.hpp>
#include <hex/api/event_manager.hpp>
#include <hex/api/shortcut_manager.hpp>
#include <hex/api/tutorial_manager.hpp>
#include <hex/ui/view.hpp>

namespace hex::plugin::builtin {

    void registerIntroductionTutorial() {
        using enum TutorialManager::Position;
        auto &tutorial = TutorialManager::createTutorial("hex.builtin.tutorial.introduction", "hex.builtin.tutorial.introduction.description");

        {
            tutorial.addStep()
                .setMessage(
                    "hex.builtin.tutorial.introduction.step1.title",
                    "hex.builtin.tutorial.introduction.step1.description",
                    Bottom | Right
                )
                .allowSkip();
        }

        {
            auto &step = tutorial.addStep();
            static EventManager::EventList::iterator eventHandle;

            step.setMessage(
                "hex.builtin.tutorial.introduction.step2.title",
                "hex.builtin.tutorial.introduction.step2.description",
                Bottom | Right
            )
            .addHighlight("hex.builtin.tutorial.introduction.step2.highlight",
            {
                "Welcome Screen/Start##SubWindow_69AA6996",
                Lang("hex.builtin.welcome.start.create_file")
            })
            .onAppear([&step] {
                eventHandle = EventProviderOpened::subscribe([&step](prv::Provider *provider) {
                    if (dynamic_cast<MemoryFileProvider*>(provider))
                        step.complete();
                });
            })
            .onComplete([] {
                EventProviderOpened::unsubscribe(eventHandle);
            });
        }

        {
            tutorial.addStep()
            .addHighlight("hex.builtin.tutorial.introduction.step3.highlight", {
                View::toWindowName("hex.builtin.view.hex_editor.name")
            })
            .allowSkip();
        }

        {
            tutorial.addStep()
            .addHighlight("hex.builtin.tutorial.introduction.step4.highlight", {
                View::toWindowName("hex.builtin.view.data_inspector.name")
            })
            .onAppear([]{
                ImHexApi::HexEditor::setSelection(Region { 0, 1 });
            })
            .allowSkip();
        }

        {
            tutorial.addStep()
            .addHighlight("hex.builtin.tutorial.introduction.step5.highlight.pattern_editor", {
                View::toWindowName("hex.builtin.view.pattern_editor.name")
            })
            .addHighlight("hex.builtin.tutorial.introduction.step5.highlight.pattern_data", {
                View::toWindowName("hex.builtin.view.pattern_data.name")
            })
            .onAppear([] {
                RequestSetPatternLanguageCode::post("\n\n\n\n\n\nstruct Test {\n    u8 value;\n};\n\nTest test @ 0x00;");
                RequestRunPatternCode::post();
            })
            .allowSkip();
        }

        {
            auto &step = tutorial.addStep();

            step.addHighlight("hex.builtin.tutorial.introduction.step6.highlight", {
                "##MainMenuBar",
                "##menubar",
                Lang("hex.builtin.menu.help")
            })
            .addHighlight({
                "##Menu_00",
                Lang("hex.builtin.view.tutorials.name")
            })
            .onAppear([&step] {
                EventViewOpened::subscribe([&step](const View *view){
                    if (view->getUnlocalizedName() == UnlocalizedString("hex.builtin.view.tutorials.name"))
                        step.complete();
                });
            })
            .onComplete([&step]{
                EventViewOpened::unsubscribe(&step);
            })
            .allowSkip();
        }
    }

}
