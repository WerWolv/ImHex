#include <hex/test/tests.hpp>

#include <hex/helpers/file.hpp>
#include <hex/helpers/fs.hpp>

using namespace std::literals::string_literals;

TEST_SEQUENCE("FileAccess") {
    const auto FilePath    = hex::fs::current_path() / "file.txt";
    const auto FileContent = "Hello World";

    {
        hex::File file(FilePath, hex::File::Mode::Create);
        TEST_ASSERT(file.isValid());

        file.write(FileContent);
    }

    {
        hex::File file(FilePath, hex::File::Mode::Read);
        TEST_ASSERT(file.isValid());

        TEST_ASSERT(file.readString() == FileContent);
    }

    {
        hex::File file(FilePath, hex::File::Mode::Write);
        TEST_ASSERT(file.isValid());


        file.remove();
        TEST_ASSERT(!file.isValid());
    }

    {
        hex::File file(FilePath, hex::File::Mode::Read);
        if (file.isValid())
            TEST_FAIL();
    }

    TEST_SUCCESS();
};