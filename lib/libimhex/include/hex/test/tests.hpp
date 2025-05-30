#pragma once

#include <hex.hpp>
#include <utility>
#include <hex/helpers/utils.hpp>
#include <hex/helpers/fmt.hpp>
#include <hex/helpers/logger.hpp>
#include <hex/api/plugin_manager.hpp>

#if defined(IMGUI_TEST_ENGINE)
    #include <imgui_te_context.h>
    #include <imgui_te_engine.h>
    #include <source_location>
    #include <hex/api/events/events_lifecycle.hpp>
#endif


#include <wolv/utils/preproc.hpp>

#include <string>
#include <map>
#include <functional>

#define TEST_SEQUENCE(...) static auto WOLV_ANONYMOUS_VARIABLE(TEST_SEQUENCE) = ::hex::test::TestSequenceExecutor(__VA_ARGS__) + []() -> int
#define TEST_FAIL()        return EXIT_FAILURE
#define TEST_SUCCESS()     return EXIT_SUCCESS
#define FAILING            true
#define TEST_ASSERT(x, ...)                                            \
    do {                                                               \
        auto ret = (x);                                                \
        if (!ret) {                                                    \
            hex::log::error("Test assert '" #x "' failed {} at {}:{}", \
                hex::format("" __VA_ARGS__),                           \
                __FILE__,                                              \
                __LINE__);                                             \
            return EXIT_FAILURE;                                       \
        }                                                              \
    } while (0)

#define INIT_PLUGIN(name) \
    if (!hex::test::initPluginImpl(name)) TEST_FAIL();

#if defined(IMGUI_TEST_ENGINE)
    #define IMGUI_TEST_SEQUENCE(category, name, ctx)                                                  \
        static auto WOLV_ANONYMOUS_VARIABLE(TEST_SEQUENCE) =                                          \
        ::hex::test::ImGuiTestSequenceExecutor(category, name, std::source_location::current()) +     \
        [](ImGuiTestContext *ctx) -> void
#else
    #define IMGUI_TEST_SEQUENCE(...) static auto WOLV_ANONYMOUS_VARIABLE(TEST_SEQUENCE) = []() -> int
#endif

namespace hex::test {

    using Function = int(*)();
    struct Test {
        Function function;
        bool shouldFail;
    };

    class Tests {
    public:
        static int addTest(const std::string &name, Function func, bool shouldFail) noexcept;

        static std::map<std::string, Test> &get() noexcept;
    };

    template<class F>
    class TestSequence {
    public:
        TestSequence(const std::string &name, F func, bool shouldFail) noexcept {
            Tests::addTest(name, func, shouldFail);
        }

        TestSequence &operator=(TestSequence &&) = delete;
    };

    struct TestSequenceExecutor {
        explicit TestSequenceExecutor(std::string name, bool shouldFail = false) noexcept : m_name(std::move(name)), m_shouldFail(shouldFail) {
        }

        [[nodiscard]] const auto &getName() const noexcept {
            return m_name;
        }

        [[nodiscard]] bool shouldFail() const noexcept {
            return m_shouldFail;
        }

    private:
        std::string m_name;
        bool m_shouldFail;
    };


    template<typename F>
    TestSequence<F> operator+(const TestSequenceExecutor &executor, F &&f) noexcept {
        return TestSequence<F>(executor.getName(), std::forward<F>(f), executor.shouldFail());
    }


    #if defined(IMGUI_TEST_ENGINE)
        template<class F>
        class ImGuiTestSequence {
        public:
            ImGuiTestSequence(const std::string &category, const std::string &name, std::source_location sourceLocation, F func) noexcept {
                log::info("Registering ImGui Test");
                EventRegisterImGuiTests::subscribe([=](ImGuiTestEngine *engine) {
                    auto test = ImGuiTestEngine_RegisterTest(engine, category.c_str(), name.c_str(), sourceLocation.file_name(), sourceLocation.line());
                    test->TestFunc = func;
                });
            }

            ImGuiTestSequence &operator=(ImGuiTestSequence &&) = delete;
        };

        struct ImGuiTestSequenceExecutor {
            explicit ImGuiTestSequenceExecutor(std::string category, std::string name, std::source_location sourceLocation) noexcept
                : m_category(std::move(category)), m_name(std::move(name)), m_sourceLocation(sourceLocation) {
            }

            [[nodiscard]] const auto &getCategory() const noexcept {
                return m_category;
            }

            [[nodiscard]] const auto &getName() const noexcept {
                return m_name;
            }

            [[nodiscard]] const auto &getSourceLocation() const noexcept {
                return m_sourceLocation;
            }

        private:
            std::string m_category, m_name;
            std::source_location m_sourceLocation;
        };


        template<typename F>
        ImGuiTestSequence<F> operator+(const ImGuiTestSequenceExecutor &executor, F &&f) noexcept {
            return ImGuiTestSequence<F>(executor.getCategory(), executor.getName(), executor.getSourceLocation(), std::forward<F>(f));
        }
    #endif


    bool initPluginImpl(std::string name);
}
