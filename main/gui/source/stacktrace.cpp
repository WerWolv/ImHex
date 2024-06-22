#include <stacktrace.hpp>
#include <hex/helpers/fmt.hpp>

#include <array>
#include <llvm/Demangle/Demangle.h>

namespace {

    [[maybe_unused]] std::string tryDemangle(const std::string &symbolName) {
        if (auto variant1 = llvm::demangle(symbolName); variant1 != symbolName)
            return variant1;

        if (auto variant2 = llvm::demangle(std::string("_") + symbolName); variant2 != std::string("_") + symbolName)
            return variant2;

        return symbolName;
    }

}

#if defined(OS_WINDOWS)

    #include <windows.h>
    #include <dbghelp.h>

    namespace hex::stacktrace {

        void initialize() {

        }

        StackTraceResult getStackTrace() {
            std::vector<StackFrame> stackTrace;

            HANDLE process = GetCurrentProcess();
            HANDLE thread = GetCurrentThread();

            CONTEXT context = {};
            context.ContextFlags = CONTEXT_FULL;
            RtlCaptureContext(&context);

            SymSetOptions(SYMOPT_CASE_INSENSITIVE | SYMOPT_UNDNAME | SYMOPT_LOAD_LINES | SYMOPT_LOAD_ANYTHING);
            SymInitialize(process, nullptr, TRUE);

            DWORD image;
            STACKFRAME64 stackFrame;
            ZeroMemory(&stackFrame, sizeof(STACKFRAME64));

            image = IMAGE_FILE_MACHINE_AMD64;
            stackFrame.AddrPC.Offset = context.Rip;
            stackFrame.AddrPC.Mode = AddrModeFlat;
            stackFrame.AddrFrame.Offset = context.Rsp;
            stackFrame.AddrFrame.Mode = AddrModeFlat;
            stackFrame.AddrStack.Offset = context.Rsp;
            stackFrame.AddrStack.Mode = AddrModeFlat;

            while (true) {
                if (StackWalk64(
                        image, process, thread,
                        &stackFrame, &context, nullptr,
                        SymFunctionTableAccess64, SymGetModuleBase64, nullptr) == FALSE)
                    break;

                if (stackFrame.AddrReturn.Offset == stackFrame.AddrPC.Offset)
                    break;

                std::array<char, sizeof(SYMBOL_INFO) + MAX_SYM_NAME * sizeof(TCHAR)> buffer = {};
                auto symbol = reinterpret_cast<PSYMBOL_INFO>(buffer.data());
                symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
                symbol->MaxNameLen = MAX_SYM_NAME;

                DWORD64 displacementSymbol = 0;
                const char *symbolName;
                if (SymFromAddr(process, stackFrame.AddrPC.Offset, &displacementSymbol, symbol) == TRUE) {
                    symbolName = symbol->Name;
                } else {
                    symbolName = "??";
                }

                SymSetOptions(SYMOPT_LOAD_LINES);

                IMAGEHLP_LINE64 line;
                line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);

                DWORD displacementLine = 0;

                u32 lineNumber = 0;
                const char *fileName;
                if (SymGetLineFromAddr64(process, stackFrame.AddrPC.Offset, &displacementLine, &line) == TRUE) {
                    lineNumber = line.LineNumber;
                    fileName = line.FileName;
                } else {
                    lineNumber = 0;
                    fileName = "??";
                }

                auto demangledName = tryDemangle(symbolName);
                stackTrace.push_back(StackFrame { fileName, demangledName, lineNumber });
            }

            SymCleanup(process);

            return StackTraceResult{ stackTrace, "StackWalk" };
        }

    }

#elif defined(HEX_HAS_EXECINFO)

    #if __has_include(BACKTRACE_HEADER)

        #include BACKTRACE_HEADER
        #include <hex/helpers/utils.hpp>
        #include <dlfcn.h>

        namespace hex::stacktrace {

            void initialize() {

            }

            StackTraceResult getStackTrace() {
                static std::vector<StackFrame> result;

                std::array<void*, 128> addresses = {};
                const size_t count = backtrace(addresses.data(), addresses.size());

                Dl_info info;
                for (size_t i = 0; i < count; i += 1) {
                    dladdr(addresses[i], &info);

                    auto fileName = info.dli_fname != nullptr ? std::fs::path(info.dli_fname).filename().string() : "??";
                    auto demangledName = info.dli_sname != nullptr ? tryDemangle(info.dli_sname) : "??";

                    result.push_back(StackFrame { std::move(fileName), std::move(demangledName), 0 });
                }

                return StackTraceResult{ result, "execinfo" };
            }

        }

    #endif

#elif defined(HEX_HAS_BACKTRACE)

    #if __has_include(BACKTRACE_HEADER)

        #include BACKTRACE_HEADER
        #include <hex/helpers/logger.hpp>
        #include <hex/helpers/utils.hpp>

        namespace hex::stacktrace {

            static struct backtrace_state *s_backtraceState;


            void initialize() {
                if (auto executablePath = wolv::io::fs::getExecutablePath(); executablePath.has_value()) {
                    static std::string path = executablePath->string();
                    s_backtraceState = backtrace_create_state(path.c_str(), 1, [](void *, const char *msg, int) { log::error("{}", msg); }, nullptr);
                }
            }

            StackTraceResult getStackTrace() {
                static std::vector<StackFrame> result;

                result.clear();
                if (s_backtraceState != nullptr) {
                    backtrace_full(s_backtraceState, 0, [](void *, uintptr_t, const char *fileName, int lineNumber, const char *function) -> int {
                        if (fileName == nullptr)
                            fileName = "??";
                        if (function == nullptr)
                            function = "??";

                        result.push_back(StackFrame { std::fs::path(fileName).filename().string(), tryDemangle(function), u32(lineNumber) });

                        return 0;
                    }, nullptr, nullptr);

                }

                return StackTraceResult{ result, "backtrace" };
            }

        }

    #endif

#else

    namespace hex::stacktrace {

        void initialize() { }
        StackTraceResult getStackTrace() {
            return StackTraceResult {
                {StackFrame { "??", "Stacktrace collecting not available!", 0 }},
                "none"
            };
        }
    }
    
#endif