#include <content/providers/memory_file_provider.hpp>
#include <hex/api/shortcut_manager.hpp>
#include <hex/api/tutorial_manager.hpp>
#include <hex/ui/view.hpp>
#include <hex/api/events/requests_interaction.hpp>
#include <hex/api/events/events_gui.hpp>

namespace hex::plugin::builtin {

    void registerIntroductionTutorial() {
        using enum TutorialManager::Position;
        auto &tutorial = TutorialManager::createTutorial("hex.builtin.tutorial.introduction"_unlocalized, "hex.builtin.tutorial.introduction.description"_unlocalized);

        {
            tutorial.addStep()
                .setMessage(
                    "hex.builtin.tutorial.introduction.step1.title"_unlocalized,
                    "hex.builtin.tutorial.introduction.step1.description"_unlocalized,
                    Bottom | Right
                )
                .allowSkip();
        }

        {
            auto &step = tutorial.addStep();
            static EventManager::EventList::iterator eventHandle;

            step.setMessage(
                "hex.builtin.tutorial.introduction.step2.title"_unlocalized,
                "hex.builtin.tutorial.introduction.step2.description"_unlocalized,
                Bottom | Right
            )
            .addHighlight("hex.builtin.tutorial.introduction.step2.highlight"_unlocalized,
            {
                "Welcome Screen/Start_087A287D",
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
            .addHighlight("hex.builtin.tutorial.introduction.step3.highlight"_unlocalized, {
                View::toWindowName("hex.builtin.view.hex_editor.name"_unlocalized)
            })
            .allowSkip();
        }

        {
            tutorial.addStep()
            .addHighlight("hex.builtin.tutorial.introduction.step4.highlight"_unlocalized, {
                View::toWindowName("hex.builtin.view.data_inspector.name"_unlocalized)
            })
            .onAppear([]{
                ImHexApi::HexEditor::setSelection(Region { .address=0, .size=1 });
            })
            .allowSkip();
        }

        {
            tutorial.addStep()
            .addHighlight("hex.builtin.tutorial.introduction.step5.highlight.pattern_editor"_unlocalized, {
                View::toWindowName("hex.builtin.view.pattern_editor.name"_unlocalized)
            })
            .addHighlight("hex.builtin.tutorial.introduction.step5.highlight.pattern_data"_unlocalized, {
                View::toWindowName("hex.builtin.view.pattern_data.name"_unlocalized)
            })
            .onAppear([] {
                RequestSetPatternLanguageCode::post("\n\n\n\n\n\nstruct Test {\n    u8 value;\n};\n\nTest test @ 0x00;");
                RequestTriggerPatternEvaluation::post();
            })
            .allowSkip();
        }

        {
            auto &step = tutorial.addStep();

            step.addHighlight("hex.builtin.tutorial.introduction.step6.highlight"_unlocalized, {
                "##MainMenuBar",
                "##MenuBar",
                Lang("hex.builtin.menu.help")
            })
            .addHighlight({
                "###Menu_00",
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
