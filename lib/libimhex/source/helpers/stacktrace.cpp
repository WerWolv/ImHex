#include <hex/helpers/stacktrace.hpp>
#include <hex/helpers/fs.hpp>

#if defined(OS_WINDOWS)

    #include <windows.h>
    #include <dbghelp.h>
    #include <llvm/Demangle/Demangle.h>

    namespace hex::stacktrace {

        void initialize() {

        }

        std::vector<StackFrame> getStackTrace() {
            std::vector<StackFrame> stackTrace;

            HANDLE process = GetCurrentProcess();
            HANDLE thread = GetCurrentThread();

            CONTEXT context;
            memset(&context, 0, sizeof(CONTEXT));
            context.ContextFlags = CONTEXT_FULL;
            RtlCaptureContext(&context);

            SymSetOptions(SYMOPT_UNDNAME | SYMOPT_LOAD_LINES);
            SymInitialize(process, nullptr, true);

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

                if (!StackWalk64(
                        image, process, thread,
                        &stackFrame, &context, nullptr,
                        SymFunctionTableAccess64, SymGetModuleBase64, nullptr))
                    break;

                char buffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME * sizeof(TCHAR)];
                auto symbol = (PSYMBOL_INFO)buffer;
                symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
                symbol->MaxNameLen = MAX_SYM_NAME;

                DWORD64 displacementSymbol = 0;
                const char* symbolName;
                if (SymFromAddr(process, stackFrame.AddrPC.Offset, &displacementSymbol, symbol)) {
                    symbolName = symbol->Name;
                } else {
                    symbolName = "??";
                }

                IMAGEHLP_LINE64 line;
                line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);

                DWORD displacementLine = 0;
                u32 lineNumber;
                const char* fileName;

                if (SymGetLineFromAddr64(process, stackFrame.AddrPC.Offset, &displacementLine, &line)) {
                    lineNumber = line.LineNumber;
                    fileName = line.FileName;
                } else {
                    lineNumber = 0;
                    fileName = "??";
                }

                std::string demangledName;
                if (auto variant1 = llvm::demangle(symbolName); variant1 != symbolName)
                    demangledName = variant1;
                else if (auto variant2 = llvm::demangle(std::string("_") + symbolName); variant2 != std::string("_") + symbolName)
                    demangledName = variant2;
                else
                    demangledName = symbolName;

                stackTrace.push_back(StackFrame { fileName, demangledName, lineNumber });

            }

            SymCleanup(process);

            return stackTrace;
        }

    }

#elif defined(HEX_HAS_EXECINFO) && __has_include(BACKTRACE_HEADER)

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