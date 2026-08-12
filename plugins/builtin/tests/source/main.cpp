#include <iostream>
#include <hex/test/tests.hpp>
#include <hex/api/plugin_manager.hpp>
#include <content/views/view_patches.hpp>
#include <hex/api/task_manager.hpp>
#include <hex/api/imhex_api/provider.hpp>
#include <hex/api/project_manager.hpp>
#include <hex/helpers/tar.hpp>
#include <content/legacy_project_importer.hpp>

#include <nlohmann/json.hpp>
#include <wolv/io/file.hpp>

using namespace hex;
using namespace hex::plugin::builtin;

TEST_SEQUENCE("Providers/ReadWrite") {
    INIT_PLUGIN("Built-in");

    auto &provider = *ImHexApi::Provider::createProvider("hex.builtin.provider.mem_file", true);

    TEST_ASSERT(provider.getSize() == 0);
    TEST_ASSERT(!provider.isDataDirty());
    TEST_ASSERT(!provider.isMetadataDirty());

    provider.resize(50);
    TEST_ASSERT(provider.getSize() == 50);
    TEST_ASSERT(provider.isDataDirty());
    TEST_ASSERT(!provider.isMetadataDirty());

    char buf[] = "\x99\x99"; // temporary value that should be overwriten
    provider.read(0, buf, 2);
    TEST_ASSERT(std::equal(buf, buf+2, "\x00\x00"));

    provider.write(0, "\xFF\xFF", 2);
    char buf2[] = "\x99\x99"; // temporary value that should be overwriten
    provider.read(0, buf2, 2);
    TEST_ASSERT(std::equal(buf2, buf2+2, "\xFF\xFF"));

    TEST_SUCCESS();
};

TEST_SEQUENCE("Providers/InvalidResize") {
    INIT_PLUGIN("Built-in");

    auto &pr = *ImHexApi::Provider::createProvider("hex.builtin.provider.mem_file", true);

    
    TEST_ASSERT(!pr.resize(-1));
    TEST_SUCCESS();
};

TEST_SEQUENCE("Project/ParseLegacy") {
    const auto projectPath = std::filesystem::current_path() / "legacy_project_test.hexproj";
    std::filesystem::remove(projectPath);

    const auto descriptor = nlohmann::json({
        { "type", "hex.builtin.provider.file" },
        { "settings", {
            { "baseAddress", 0 },
            { "currPage", 0 },
            { "path", "data.bin" }
        } }
    });
    {
        Tar tar(projectPath, Tar::Mode::Create);
        TEST_ASSERT(tar.isValid());
        tar.writeString("IMHEX_METADATA", "HEX\n1.39.0");
        nlohmann::json manifest;
        manifest["providers"] = std::vector<u32> { 7 };
        tar.writeString("providers/providers.json", manifest.dump());
        tar.writeString("providers/7.json", descriptor.dump());
        tar.writeString("7/bookmarks.json", R"({"bookmarks":[]})");
        tar.writeString("7/custom_encoding.tbl", "41=A");
        tar.writeString("7/patches.json", R"({"patches":{"16":255}})");
    }

    const auto parsed = parseLegacyProject(projectPath);
    TEST_ASSERT(parsed.isValid(), "{}", parsed.error);
    TEST_ASSERT(parsed.providers.size() == 1);
    TEST_ASSERT(parsed.providers.front().id == 7);
    TEST_ASSERT(parsed.providers.front().files.size() == 2);
    TEST_ASSERT(parsed.providers.front().files[0].typeId == "hex.builtin.bookmarks");
    TEST_ASSERT(parsed.providers.front().files[1].typeId == "hex.builtin.custom-encoding");
    TEST_ASSERT(parsed.providers.front().patches.at(16) == 0xFF);
    TEST_ASSERT(parsed.projectFiles.size() == 1);
    TEST_ASSERT(parsed.projectFiles.front().name == "legacy-provider-7-patches.json");

    const auto importedPath = std::filesystem::path(
        parsed.providers.front().descriptor["settings"]["path"].get<std::string>());
    TEST_ASSERT(importedPath == (projectPath.parent_path() / "data.bin").lexically_normal());

    std::filesystem::remove(projectPath);
    TEST_SUCCESS();
};

TEST_SEQUENCE("Project/ImportLegacy") {
    INIT_PLUGIN("Built-in");

    const auto root = std::filesystem::current_path() / "legacy_project_import";
    const auto projectPath = std::filesystem::current_path() / "legacy_project_import.hexproj";
    std::filesystem::remove_all(root);
    std::filesystem::remove(projectPath);
    std::filesystem::create_directory(root);

    nlohmann::json descriptor = {
        { "type", "hex.builtin.provider.mem_file" },
        { "settings", {
            { "baseAddress", 0 },
            { "currPage", 0 },
            { "data", std::vector<u8> { 0x01, 0x02, 0x03 } },
            { "name", "Imported" },
            { "readOnly", false }
        } }
    };
    {
        Tar tar(projectPath, Tar::Mode::Create);
        nlohmann::json manifest;
        manifest["providers"] = std::vector<u32> { 11 };
        tar.writeString("IMHEX_METADATA", "HEX\n1.39.0");
        tar.writeString("providers/providers.json", manifest.dump());
        tar.writeString("providers/11.json", descriptor.dump());
        tar.writeString("11/bookmarks.json", R"({"bookmarks":[]})");
    }

    TEST_ASSERT(ProjectManager::load(root));
    const auto imported = importLegacyProject(projectPath);
    TEST_ASSERT(imported.success, "{}", imported.error);
    TEST_ASSERT(imported.importedProviderCount == 1);
    TEST_ASSERT(imported.importedFileCount == 1);
    TEST_ASSERT(imported.failedProviderIds.empty());

    const auto providers = ImHexApi::Provider::getProviders();
    TEST_ASSERT(providers.size() == 1);
    TEST_ASSERT(providers.front()->getActualSize() == 3);

    const auto projectSettings = nlohmann::json::parse(
        wolv::io::File(root / ".imhex/project.json", wolv::io::File::Mode::Read).readString());
    const auto providerId = std::to_string(providers.front()->getID());
    const auto relativePath = std::filesystem::path(
        projectSettings["associations"][providerId]["hex.builtin.bookmarks"].get<std::string>());
    TEST_ASSERT(std::filesystem::is_regular_file(root / relativePath));
    TEST_ASSERT(wolv::io::File(root / relativePath, wolv::io::File::Mode::Read).readString() == R"({"bookmarks":[]})");

    std::filesystem::remove(projectPath);
    TEST_SUCCESS();
};
