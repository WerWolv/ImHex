#include <hex/test/tests.hpp>

#include <hex/api/imhex_api/system.hpp>

// We only declare the CLI runner here; it's defined in the GUI main module
namespace hex::init {
    void runCommandLine(int argc, char **argv);
}

static int runReadOnlyCLI(const std::vector<const char*> &args) {
    int argc = static_cast<int>(args.size());
    // const_cast is safe here because CLI layer doesn't mutate program name/args strings
    auto argv = const_cast<char**>(reinterpret_cast<char* const*>(args.data()));
    hex::init::runCommandLine(argc, argv);
    return EXIT_SUCCESS;
}

TEST_SEQUENCE("ReadOnlyFlagSetsMode") {
    // Simulate: imhex --readonly somefile
    const char *prog = "imhex";
    const char *flag = "--readonly";
    const char *file = "dummy.bin";
    const std::vector<const char*> argv = { prog, flag, file };

    (void) runReadOnlyCLI(argv);

    // Expect the global read-only mode to be enabled
    TEST_ASSERT(hex::ImHexApi::System::isReadOnlyMode());

    TEST_SUCCESS();
};


