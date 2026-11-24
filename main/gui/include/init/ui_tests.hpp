#pragma once

struct ImGuiTestEngine;

namespace hex::init {

    bool uiTestsEnabled();

    // Runs registered ImGui tests during the normal application lifecycle.
    class UiTestRunner {
    public:
        UiTestRunner();
        ~UiTestRunner();

        [[nodiscard]] int result() const;

    private:
        ImGuiTestEngine *m_engine = nullptr;
        bool m_started = false;
        int m_result = 1;
    };

}
