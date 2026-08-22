#include <hex/test/tests.hpp>

#include <wolv/io/file.hpp>
#include <hex/providers/file_backed_provider_data.hpp>
#include <hex/test/test_provider.hpp>
#include <hex/api/localization_manager.hpp>

#include <charconv>
#include <chrono>
#include <thread>

using namespace std::literals::string_literals;
using namespace hex;

TEST_SEQUENCE("FileAccess") {
    const auto FilePath    = std::fs::current_path() / "file.txt";
    const auto FileContent = "Hello World";

    std::fs::create_directories(FilePath.parent_path());

    {
        wolv::io::File file(FilePath, wolv::io::File::Mode::Create);
        TEST_ASSERT(file.isValid());

        file.writeString(FileContent);
    }

    {
        wolv::io::File file(FilePath, wolv::io::File::Mode::Read);
        TEST_ASSERT(file.isValid());

        TEST_ASSERT(file.readString() == FileContent);
    }

    {
        wolv::io::File file(FilePath, wolv::io::File::Mode::Write);
        TEST_ASSERT(file.isValid());


        file.remove();
        TEST_ASSERT(!file.isValid());
    }

    {
        wolv::io::File file(FilePath, wolv::io::File::Mode::Read);
        if (file.isValid())
            TEST_FAIL();
    }

    TEST_SUCCESS();
};

TEST_SEQUENCE("FileBackedProviderData") {
    using namespace std::chrono_literals;

    const auto filePath = std::fs::current_path() / "file_backed_provider_data.txt";
    std::fs::remove(filePath);

    std::vector<u8> providerBytes;
    test::TestProvider provider(&providerBytes);
    test::TestProvider secondProvider(&providerBytes);
    u32 encodeCalls = 0;
    FileBackedProviderData<int> data({
        .typeId = "hex.test.file_backed_provider_data",
        .displayName = "Test data"_untranslated,
        .displayIcon = "T",
        .extensions = { { "Test data", "txt" } },
        .encode = [&encodeCalls](const int &value) {
            encodeCalls += 1;
            const auto text = std::to_string(value);
            return std::vector<u8>(text.begin(), text.end());
        },
        .decode = [](std::span<const u8> bytes) -> std::optional<int> {
            int result = 0;
            const auto *begin = reinterpret_cast<const char *>(bytes.data());
            const auto *end = begin + bytes.size();
            const auto [position, error] = std::from_chars(begin, end, result);
            if (error != std::errc() || position != end)
                return std::nullopt;
            return result;
        },
        .createDefault = [] { return 7; },
        .debounce = 0ms
    });
    u32 notifications = 0;
    data.setChangedCallback([&](prv::Provider *changedProvider) {
        if (changedProvider == &provider)
            notifications += 1;
    });

    TEST_ASSERT(data.get(&provider) == 7);
    const auto &constData = data;
    TEST_ASSERT(constData.get(&secondProvider) == 7);
    const auto createdFilePath = std::fs::current_path() / "file_backed_provider_data_created.txt";
    std::fs::remove(createdFilePath);
    TEST_ASSERT(FileBackedProviderDataRegistry::createFile(data.getType().typeId, createdFilePath));
    {
        wolv::io::File file(createdFilePath, wolv::io::File::Mode::Read);
        TEST_ASSERT(file.readString() == "7");
        file.remove();
    }
    data.set(11, &secondProvider);
    TEST_ASSERT(data.hasPendingData(&secondProvider));
    TEST_ASSERT(!secondProvider.isMetadataDirty());
    TEST_ASSERT(!data.isBound(&secondProvider));
    TEST_ASSERT(FileBackedProviderDataRegistry::get("hex.test.file_backed_provider_data") == &data);
    TEST_ASSERT(FileBackedProviderDataRegistry::bind(&provider, data.getType().typeId, filePath));
    TEST_ASSERT(FileBackedProviderDataRegistry::isBound(&provider, data.getType().typeId));

    const auto encodeCallsBeforeIdleFrames = encodeCalls;
    EventFrameEnd::post();
    EventFrameEnd::post();
    EventFrameEnd::post();
    TEST_ASSERT(encodeCalls == encodeCallsBeforeIdleFrames);

    data.set(42, &provider);
    EventFrameEnd::post();
    {
        wolv::io::File file(filePath, wolv::io::File::Mode::Read);
        TEST_ASSERT(file.isValid());
        TEST_ASSERT(file.readString() == "42");
    }

    const auto relocatedFilePath = std::fs::current_path() / "file_backed_provider_data_relocated.txt";
    std::fs::remove(relocatedFilePath);
    data.set(43, &provider);
    std::fs::rename(filePath, relocatedFilePath);
    TEST_ASSERT(FileBackedProviderDataRegistry::relocate(&provider, data.getType().typeId, relocatedFilePath));
    EventFrameEnd::post();
    {
        wolv::io::File file(relocatedFilePath, wolv::io::File::Mode::Read);
        TEST_ASSERT(file.readString() == "43");
    }
    std::fs::rename(relocatedFilePath, filePath);
    TEST_ASSERT(FileBackedProviderDataRegistry::relocate(&provider, data.getType().typeId, filePath));

    std::this_thread::sleep_for(100ms);
    {
        wolv::io::File file(filePath, wolv::io::File::Mode::Create);
        TEST_ASSERT(file.writeString("84") == 2);
    }

    const auto reloadDeadline = std::chrono::steady_clock::now() + 2500ms;
    while (data.get(&provider) != 84 && std::chrono::steady_clock::now() < reloadDeadline) {
        std::this_thread::sleep_for(50ms);
        EventFrameEnd::post();
    }
    TEST_ASSERT(data.get(&provider) == 84);

    std::fs::remove(filePath);
    const auto deletionDeadline = std::chrono::steady_clock::now() + 2500ms;
    while (data.get(&provider) != 7 && std::chrono::steady_clock::now() < deletionDeadline) {
        std::this_thread::sleep_for(50ms);
        EventFrameEnd::post();
    }
    TEST_ASSERT(data.get(&provider) == 7);
    TEST_ASSERT(data.isBound(&provider));

    {
        wolv::io::File file(filePath, wolv::io::File::Mode::Create);
        TEST_ASSERT(file.writeString("21") == 2);
    }
    const auto recreationDeadline = std::chrono::steady_clock::now() + 2500ms;
    while (data.get(&provider) != 21 && std::chrono::steady_clock::now() < recreationDeadline) {
        std::this_thread::sleep_for(50ms);
        EventFrameEnd::post();
    }
    TEST_ASSERT(data.get(&provider) == 21);

    const auto notificationsBeforeUnbind = notifications;
    TEST_ASSERT(FileBackedProviderDataRegistry::unbind(&provider, data.getType().typeId));
    TEST_ASSERT(notifications == notificationsBeforeUnbind + 1);
    TEST_ASSERT(data.get(&provider) == 7);
    TEST_ASSERT(!FileBackedProviderDataRegistry::getBinding(&provider, data.getType().typeId).has_value());

    data.get(&provider) = 9;
    const auto encodeCallsBeforeUnmarkedChange = encodeCalls;
    EventFrameEnd::post();
    TEST_ASSERT(data.get(&provider) == 9);
    TEST_ASSERT(encodeCalls == encodeCallsBeforeUnmarkedChange);
    data.markChanged(&provider);
    TEST_ASSERT(encodeCalls == encodeCallsBeforeUnmarkedChange);
    {
        wolv::io::File file(filePath, wolv::io::File::Mode::Read);
        TEST_ASSERT(file.readString() == "21");
        file.remove();
    }

    TEST_SUCCESS();
};

TEST_SEQUENCE("UTF-8 Path") {
    const auto FilePath    = std::fs::current_path() / u8"读写汉字" / u8"привет.txt";
    const auto FileContent = u8"שלום עולם";

    std::fs::create_directories(FilePath.parent_path());

    {
        wolv::io::File file(FilePath, wolv::io::File::Mode::Create);
        TEST_ASSERT(file.isValid());

        file.writeU8String(FileContent);
    }

    {
        wolv::io::File file(FilePath, wolv::io::File::Mode::Read);
        TEST_ASSERT(file.isValid());

        TEST_ASSERT(file.readU8String() == FileContent);
    }

    {
        wolv::io::File file(FilePath, wolv::io::File::Mode::Write);
        TEST_ASSERT(file.isValid());


        file.remove();
        TEST_ASSERT(!file.isValid());
    }

    {
        wolv::io::File file(FilePath, wolv::io::File::Mode::Read);
        if (file.isValid())
            TEST_FAIL();
    }

    TEST_SUCCESS();
};
