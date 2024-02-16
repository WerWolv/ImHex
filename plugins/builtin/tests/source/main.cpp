#include <iostream>
#include <hex/test/tests.hpp>
#include <hex/api/plugin_manager.hpp>
#include <content/providers/memory_file_provider.hpp>
#include <content/views/view_patches.hpp>
#include <hex/api/task_manager.hpp>

using namespace hex;
using namespace hex::plugin::builtin;

TEST_SEQUENCE("Providers/ReadWrite") {
    INIT_PLUGIN("Built-in");

    auto &pr = *ImHexApi::Provider::createProvider("hex.builtin.provider.mem_file", true);

    TEST_ASSERT(pr.getSize() == 0);
    TEST_ASSERT(!pr.isDirty());

    pr.resize(50);
    TEST_ASSERT(pr.getSize() == 50);
    TEST_ASSERT(pr.isDirty());

    char buf[] = "\x99\x99"; // temporary value that should be overwriten
    pr.read(0, buf, 2);
    TEST_ASSERT(std::equal(buf, buf+2, "\x00\x00"));

    pr.write(0, "\xFF\xFF", 2);
    char buf2[] = "\x99\x99"; // temporary value that should be overwriten
    pr.read(0, buf2, 2);
    TEST_ASSERT(std::equal(buf2, buf2+2, "\xFF\xFF"));

    TEST_SUCCESS();
};

TEST_SEQUENCE("Providers/InvalidResize") {
    INIT_PLUGIN("Built-in");

    auto &pr = *ImHexApi::Provider::createProvider("hex.builtin.provider.mem_file", true);

    
    TEST_ASSERT(!pr.resize(-1));
    TEST_SUCCESS();
};