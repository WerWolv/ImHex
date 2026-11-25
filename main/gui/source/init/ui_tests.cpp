#include <init/ui_tests.hpp>

#include <hex/api/events/events_gui.hpp>
#include <hex/api/events/events_lifecycle.hpp>
#include <hex/api/events/requests_lifecycle.hpp>
#include <hex/api/imhex_api/system.hpp>

#include <cstdio>
#include <cstdlib>

#if defined(IMGUI_TEST_ENGINE)
    #include <imgui_te_engine.h>
#endif

namespace hex::init {

    bool uiTestsEnabled() {
        return ImHexApi::System::getInitArguments().contains("run-ui-tests");
    }

    UiTestRunner::UiTestRunner() {
        #if defined(IMGUI_TEST_ENGINE)
            if (!uiTestsEnabled())
                return;

            EventRegisterImGuiTests::subscribe(this, [this](ImGuiTestEngine *engine) {
                m_engine = engine;
            });
            EventImHexStartupFinished::subscribe(this, [this] {
                auto *engine = m_engine;
                ImGuiTestEngine_GetIO(engine).ConfigLogToTTY = true;

                ImVector<ImGuiTest *> tests;
                ImGuiTestEngine_GetTestList(engine, &tests);
                const auto filter = ImHexApi::System::getInitArgument("run-ui-tests");
                for (auto *test : tests) {
                    if (test->Group == ImGuiTestGroup_Tests &&
                        (filter.empty() || filter == test->Name))
                        ImGuiTestEngine_QueueTest(engine, test);
                }
                m_started = true;
            });
            EventFrameEnd::subscribe(this, [this] {
                auto *engine = m_engine;
                if (!m_started || !ImGuiTestEngine_IsTestQueueEmpty(engine))
                    return;

                ImGuiTestEngineResultSummary result;
                ImGuiTestEngine_GetResultSummary(engine, &result);
                std::fprintf(stderr, "UI test result: %d of %d passed\n", result.CountSuccess, result.CountTested);
                const auto filter = ImHexApi::System::getInitArgument("run-ui-tests");
                if (result.CountTested == 0 && !filter.empty())
                    std::fprintf(stderr, "No UI test named '%s' was found\n", filter.c_str());
                m_result = result.CountTested > 0 && result.CountSuccess == result.CountTested ? EXIT_SUCCESS : EXIT_FAILURE;
                m_started = false;
                RequestCloseImHex::post(true);
            });
        #endif
    }

    UiTestRunner::~UiTestRunner() {
        #if defined(IMGUI_TEST_ENGINE)
            if (uiTestsEnabled()) {
                EventRegisterImGuiTests::unsubscribe(this);
                EventImHexStartupFinished::unsubscribe(this);
                EventFrameEnd::unsubscribe(this);
            }
        #endif
    }

    int UiTestRunner::result() const {
        return m_result;
    }

}
