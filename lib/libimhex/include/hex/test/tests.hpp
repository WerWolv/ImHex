#pragma once

#include <hex.hpp>
#include <utility>
#include <hex/helpers/utils.hpp>
#include <hex/helpers/fmt.hpp>
#include <hex/helpers/logger.hpp>

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

namespace hex::test {

    using Function = int(*)();
    struct Test {
        Function function;
        bool shouldFail;
    };

    class Tests {
    public:
        static auto addTest(const std::string &name, Function func, bool shouldFail) noexcept {
            s_tests.insert({
                name, {func, shouldFail}
            });

            return 0;
        }

        static auto &get() noexcept {
            return s_tests;
        }

    private:
        static std::map<std::string, Test> s_tests;
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

}
