#include <iostream>
#include <mutex>
#include <hex/trace/stacktrace.hpp>

#include <llvm/Demangle/Demangle.h>

namespace hex::trace {

    std::string demangle(const std::string &symbolName) {
        if (auto result = llvm::demangle(symbolName); result != symbolName)
            return result;

        if (auto result = llvm::demangle(std::string("_") + symbolName); result != std::string("_") + symbolName)
            return result;

        if (auto result = llvm::demangle(std::string("_Z") + symbolName); result != std::string("_Z") + symbolName)
            return result;

        return symbolName;
    }

}

static std::mutex s_traceMutex;

#if defined(HEX_HAS_STD_STACKTRACE) && __has_include(<stacktrace>)

    #include <stacktrace>
    
    #if __has_include(<dlfcn.h>)

        #include <filesystem>
        #include <dlfcn.h>
        #include <fmt/format.h>

    #endif

    namespace hex::trace {

        static std::string toUTF8String(const auto &value) {
            auto result = value.generic_u8string();

            return { result.begin(), result.end() };
        }

        void initialize() {

        }

        StackTraceResult getStackTrace() {
            std::lock_guard lock(s_traceMutex);

            StackTrace result;

            auto stackTrace = std::stacktrace::current();

            for (const auto &entry : stackTrace) {
                if (entry.source_line() == 0 && entry.source_file().empty()) {
                    #if __has_include(<dlfcn.h>)
                        Dl_info info = {};
                        dladdr(reinterpret_cast<const void*>(entry.native_handle()), &info);

                        std::string description;

                        auto path = info.dli_fname != nullptr ? std::optional<std::filesystem::path>{info.dli_fname} : std::nullopt;
                        auto filePath = path ? toUTF8String(*path) : "??";
                        auto fileName = path ? toUTF8String(path->filename()) : "";

                        if (info.dli_sname != nullptr) {
                            description = demangle(info.dli_sname);
                            if (info.dli_saddr != reinterpret_cast<const void*>(entry.native_handle())) {
                                auto symOffset = entry.native_handle() - reinterpret_cast<uintptr_t>(info.dli_saddr);
                                description += fmt::format("+0x{:x}", symOffset);
                            }
                        } else {
                            auto rvaOffset = entry.native_handle() - reinterpret_cast<uintptr_t>(info.dli_fbase);
                            description = fmt::format("{}+0x{:08x}", fileName, rvaOffset);
                        }

                        result.emplace_back(filePath, description, 0);
                    #else
                        result.emplace_back("", "??", 0);
                    #endif
                } else {
                    result.emplace_back(entry.source_file(), entry.description(), entry.source_line());
                }
            }

            return { result, "std::stacktrace" };
        }

    }

#elif defined(OS_WINDOWS)

    #include <windows.h>
    #include <dbghelp.h>
    #include <array>

    namespace hex::trace {

        void initialize() {

        }

        StackTraceResult getStackTrace() {
            std::lock_guard lock(s_traceMutex);

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

            #if defined(_X86_)
                image = IMAGE_FILE_MACHINE_I386;
                stackFrame.AddrPC.Offset = context.Eip;
                stackFrame.AddrPC.Mode = AddrModeFlat;
                stackFrame.AddrFrame.Offset = context.Esp;
                stackFrame.AddrFrame.Mode = AddrModeFlat;
                stackFrame.AddrStack.Offset = context.Esp;
                stackFrame.AddrStack.Mode = AddrModeFlat;
            #elif defined(_ARM64_)
                image = IMAGE_FILE_MACHINE_ARM64;
                stackFrame.AddrPC.Offset = context.Pc;
                stackFrame.AddrPC.Mode = AddrModeFlat;
                stackFrame.AddrFrame.Offset = context.Sp;
                stackFrame.AddrFrame.Mode = AddrModeFlat;
                stackFrame.AddrStack.Offset = context.Sp;
                stackFrame.AddrStack.Mode = AddrModeFlat;
            #elif defined(_AMD64_)
                image = IMAGE_FILE_MACHINE_AMD64;
                stackFrame.AddrPC.Offset = context.Rip;
                stackFrame.AddrPC.Mode = AddrModeFlat;
                stackFrame.AddrFrame.Offset = context.Rsp;
                stackFrame.AddrFrame.Mode = AddrModeFlat;
                stackFrame.AddrStack.Offset = context.Rsp;
                stackFrame.AddrStack.Mode = AddrModeFlat;
            #else
                #warning "Unsupported architecture! Add support for your architecture here."
                return {};
            #endif

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

                std::uint32_t lineNumber = 0;
                const char *fileName;
                if (SymGetLineFromAddr64(process, stackFrame.AddrPC.Offset, &displacementLine, &line) == TRUE) {
                    lineNumber = line.LineNumber;
                    fileName = line.FileName;
                } else {
                    lineNumber = 0;
                    fileName = "??";
                }

                auto demangledName = demangle(symbolName);
                stackTrace.push_back(StackFrame { fileName, demangledName, lineNumber });
            }

            SymCleanup(process);

            return StackTraceResult{ stackTrace, "StackWalk" };
        }

    }

#elif defined(HEX_HAS_EXECINFO)

    #if __has_include(BACKTRACE_HEADER)

        #include BACKTRACE_HEADER
        #include <filesystem>
        #include <dlfcn.h>
        #include <array>

        namespace hex::trace {

            void initialize() {

            }

            StackTraceResult getStackTrace() {
                std::scoped_lock lock(s_traceMutex);

                static std::vector<StackFrame> result;

                std::array<void*, 128> addresses = {};
                const size_t count = backtrace(addresses.data(), addresses.size());

                Dl_info info;
                for (size_t i = 0; i < count; i += 1) {
                    dladdr(addresses[i], &info);

                    auto fileName = info.dli_fname != nullptr ? std::filesystem::path(info.dli_fname).filename().string() : "??";
                    auto demangledName = info.dli_sname != nullptr ? demangle(info.dli_sname) : "??";

                    result.push_back(StackFrame { .file=std::move(fileName), .function=std::move(demangledName), .line=0 });
                }

                return StackTraceResult{ .stackFrames=result, .implementationName="execinfo" };
            }

        }

    #endif

#elif defined(HEX_HAS_BACKTRACE)

    #if __has_include(BACKTRACE_HEADER)

        #include BACKTRACE_HEADER

        #include <wolv/io/fs.hpp>

        namespace hex::trace {

            static struct backtrace_state *s_backtraceState;


            void initialize() {
                std::lock_guard lock(s_traceMutex);

                if (auto executablePath = wolv::io::fs::getExecutablePath(); executablePath.has_value()) {
                    static std::string path = executablePath->string();
                    s_backtraceState = backtrace_create_state(path.c_str(), 1, [](void *, const char *, int) { }, nullptr);
                }
            }

            StackTraceResult getStackTrace() {
                std::lock_guard lock(s_traceMutex);

                static std::vector<StackFrame> result;

                if (s_backtraceState != nullptr) {
                    backtrace_full(s_backtraceState, 0, [](void *, uintptr_t, const char *fileName, int lineNumber, const char *function) -> int {
                        if (fileName == nullptr)
                            fileName = "??";
                        if (function == nullptr)
                            function = "??";

                        result.push_back(StackFrame { std::filesystem::path(fileName).filename().string(), demangle(function), std::uint32_t(lineNumber) });

                        return 0;
                    }, nullptr, nullptr);

                }

                return StackTraceResult{ result, "backtrace" };
            }

        }

    #endif

#else

    namespace hex::trace {

        void initialize() { }
        StackTraceResult getStackTrace() {
            std::lock_guard lock(s_traceMutex);

            return StackTraceResult {
                {StackFrame { "??", "Stacktrace collecting not available!", 0 }},
                "none"
            };
        }
    }
    
#endif
