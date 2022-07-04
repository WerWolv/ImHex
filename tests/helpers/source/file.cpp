#include <hex/test/tests.hpp>

#include <hex/helpers/file.hpp>
#include <hex/helpers/fs.hpp>

using namespace std::literals::string_literals;

TEST_SEQUENCE("FileAccess") {
    const auto FilePath    = std::fs::current_path() / "file.txt";
    const auto FileContent = "Hello World";

    std::fs::create_directories(FilePath.parent_path());

    {
        hex::fs::File file(FilePath, hex::fs::File::Mode::Create);
        TEST_ASSERT(file.isValid());

        file.write(FileContent);
    }

    {
        hex::fs::File file(FilePath, hex::fs::File::Mode::Read);
        TEST_ASSERT(file.isValid());

        TEST_ASSERT(file.readString() == FileContent);
    }

    {
        hex::fs::File file(FilePath, hex::fs::File::Mode::Write);
        TEST_ASSERT(file.isValid());


        file.remove();
        TEST_ASSERT(!file.isValid());
    }

    {
        hex::fs::File file(FilePath, hex::fs::File::Mode::Read);
        if (file.isValid())
            TEST_FAIL();
    }

    TEST_SUCCESS();
};

TEST_SEQUENCE("UTF-8 Path") {
    const auto FilePath    = std::fs::current_path() / u8"读写汉字" / u8"привет.txt";
    const auto FileContent = u8"שלום עולם";

    std::fs::create_directories(FilePath.parent_path());

    {
        hex::fs::File file(FilePath, hex::fs::File::Mode::Create);
        TEST_ASSERT(file.isValid());

        file.write(FileContent);
    }

    {
        hex::fs::File file(FilePath, hex::fs::File::Mode::Read);
        TEST_ASSERT(file.isValid());

        TEST_ASSERT(file.readU8String() == FileContent);
    }

    {
        hex::fs::File file(FilePath, hex::fs::File::Mode::Write);
        TEST_ASSERT(file.isValid());


        file.remove();
        TEST_ASSERT(!file.isValid());
    }

    {
        hex::fs::File file(FilePath, hex::fs::File::Mode::Read);
        if (file.isValid())
            TEST_FAIL();
    }

    TEST_SUCCESS();
};