#pragma once

#include <hex.hpp>

#include <hex/api/task_manager.hpp>
#include <hex/helpers/literals.hpp>
#include <hex/helpers/fs.hpp>
#include <hex/providers/matchers/base_matcher.hpp>

#include <string>
#include <vector>


namespace hex::prv {
    class Provider;
}

namespace hex::magic {

    using namespace hex::literals;

    bool compile();
    std::string getDescription(const std::vector<u8> &data, bool firstEntryOnly = false);
    std::string getDescription(prv::Provider *provider, u64 address = 0x00, size_t size = 100_KiB, bool firstEntryOnly = false);
    std::string getMIMEType(const std::vector<u8> &data, bool firstEntryOnly = false);
    std::string getMIMEType(prv::Provider *provider, u64 address = 0x00, size_t size = 100_KiB, bool firstEntryOnly = false);
    std::string getExtensions(const std::vector<u8> &data, bool firstEntryOnly = false);
    std::string getExtensions(prv::Provider *provider, u64 address = 0x00, size_t size = 100_KiB, bool firstEntryOnly = false);
    std::string getAppleCreatorType(const std::vector<u8> &data, bool firstEntryOnly = false);
    std::string getAppleCreatorType(prv::Provider *provider, u64 address = 0x00, size_t size = 100_KiB, bool firstEntryOnly = false);

    bool isValidMIMEType(const std::string &mimeType);

    struct FoundPattern {
        std::fs::path patternFilePath;
        std::string author;
        std::string description;
        std::shared_ptr<prv::PatternMatcherBase> matcher;
    };

    std::vector<FoundPattern> findViablePatterns(prv::Provider *provider, Task* task = nullptr);

}