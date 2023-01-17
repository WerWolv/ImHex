#include <hex/helpers/stacktrace.hpp>
#include <hex/helpers/fs.hpp>

#if defined(HEX_HAS_EXECINFO) && __has_include(BACKTRACE_HEADER)

    #include BACKTRACE_HEADER
    #include <llvm/Demangle/Demangle.h>
    #include <hex/helpers/logger.hpp>
    #include <hex/helpers/utils.hpp>

    namespace hex::stacktrace {

        void initialize() {

        }

        std::vector<StackFrame> getStackTrace() {
            static std::vector<StackFrame> result;

            std::array<void*, 128> addresses;
            auto count = backtrace(addresses.data(), addresses.size());
            auto functions = backtrace_symbols(addresses.data(), count);

            for (i32 i = 0; i < count; i++)
                result.push_back(StackFrame { "", functions[i], 0 });

            return result;
        }

    }

#elif defined(HEX_HAS_BACKTRACE) && __has_include(BACKTRACE_HEADER)

    #include BACKTRACE_HEADER
    #include <llvm/Demangle/Demangle.h>
    #include <hex/helpers/logger.hpp>
    #include <hex/helpers/utils.hpp>

    namespace hex::stacktrace {

        static struct backtrace_state *s_backtraceState;


        void initialize() {
            if (auto executablePath = fs::getExecutablePath(); executablePath.has_value()) {
                static std::string path = executablePath->string();
                s_backtraceState = backtrace_create_state(path.c_str(), 1, [](void *, const char *msg, int) { log::error("{}", msg); }, nullptr);
            }
        }

        std::vector<StackFrame> getStackTrace() {
            static std::vector<StackFrame> result;

            result.clear();
            if (s_backtraceState != nullptr) {
                backtrace_full(s_backtraceState, 0, [](void *, uintptr_t, const char *fileName, int lineNumber, const char *function) -> int {
                    if (fileName == nullptr)
                        fileName = "??";
                    if (function == nullptr)
                        function = "??";

                    result.push_back(StackFrame { std::fs::path(fileName).filename().string(), llvm::demangle(function), u32(lineNumber) });

                    return 0;
                }, nullptr, nullptr);

            }

            return result;
        }

    }

#else

    namespace hex::stacktrace {

        void initialize() { }
        std::vector<StackFrame> getStackTrace() { return { }; }

    }
    
#endif