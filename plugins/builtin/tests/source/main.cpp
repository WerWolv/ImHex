#include <iostream>
#include <hex/test/tests.hpp>
#include <hex/api/plugin_manager.hpp>
#include <content/views/view_patches.hpp>
#include <hex/api/task_manager.hpp>
#include <hex/api/imhex_api/provider.hpp>
#include <hex/api/project_manager.hpp>
#include <hex/helpers/tar.hpp>
#include <content/legacy_project_importer.hpp>
#include <content/recent.hpp>

#include <nlohmann/json.hpp>
#include <wolv/io/file.hpp>

using namespace hex;
using namespace hex::plugin::builtin;

TEST_SEQUENCE("Providers/ReadWrite") {
    INIT_PLUGIN("Built-in");

    auto &provider = *ImHexApi::Provider::createProvider("hex.builtin.provider.mem_file"_unlocalized, true);

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

    auto &pr = *ImHexApi::Provider::createProvider("hex.builtin.provider.mem_file"_unlocalized, true);

    
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
    TEST_ASSERT(isLegacyProjectFile(projectPath));
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

    const auto projectFolder = std::filesystem::current_path() / "legacy_folder.hexproj";
    std::filesystem::remove_all(projectFolder);
    std::filesystem::create_directory(projectFolder);
    TEST_ASSERT(!isLegacyProjectFile(projectFolder));

    std::filesystem::remove(projectPath);
    std::filesystem::remove(projectFolder);
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

TEST_SEQUENCE("Project/MigrateLegacy") {
    INIT_PLUGIN("Built-in");

    const auto root = std::filesystem::current_path() / "legacy_project_migrated";
    const auto projectPath = std::filesystem::current_path() / "legacy_project_migration.hexproj";
    std::filesystem::remove_all(root);
    std::filesystem::remove(projectPath);
    std::filesystem::create_directory(root);

    const nlohmann::json descriptor = {
        { "type", "hex.builtin.provider.mem_file" },
        { "settings", {
            { "baseAddress", 0 },
            { "currPage", 0 },
            { "data", std::vector<u8> { 0xCA, 0xFE } },
            { "name", "Migrated" },
            { "readOnly", false }
        } }
    };
    {
        Tar tar(projectPath, Tar::Mode::Create);
        nlohmann::json manifest;
        manifest["providers"] = std::vector<u32> { 17 };
        tar.writeString("IMHEX_METADATA", "HEX\n1.39.0");
        tar.writeString("providers/providers.json", manifest.dump());
        tar.writeString("providers/17.json", descriptor.dump());
        tar.writeString("17/bookmarks.json", R"({"bookmarks":[]})");
    }

    const auto migrated = migrateLegacyProject(projectPath, root);
    TEST_ASSERT(migrated.success, "{}", migrated.error);
    TEST_ASSERT(ProjectManager::isFolderProject());
    TEST_ASSERT(ProjectManager::getPath() == root);
    TEST_ASSERT(std::filesystem::is_regular_file(root / ".imhex/project.json"));
    TEST_ASSERT(std::filesystem::is_regular_file(root / ".imhex/providers/providers.json"));
    TEST_ASSERT(migrated.importedProviderCount == 1);
    TEST_ASSERT(migrated.importedFileCount == 1);

    const auto providers = ImHexApi::Provider::getProviders();
    TEST_ASSERT(providers.size() == 1);
    TEST_ASSERT(providers.front()->getActualSize() == 2);
    TEST_ASSERT(std::filesystem::is_regular_file(projectPath));

    TEST_ASSERT(project::createEmptyProject(root) == std::string("hex.builtin.popup.error.project.create.metadata_exists"_lang));

    std::filesystem::remove(projectPath);
    TEST_SUCCESS();
};

TEST_SEQUENCE("Project/ProviderOpenState") {
    INIT_PLUGIN("Built-in");

    const auto root = std::filesystem::current_path() / "project_provider_open_state";
    const auto backupRoot = std::filesystem::current_path() / "project_provider_open_state_backup";
    const auto metadataRoot = root / ".imhex";
    const auto providersRoot = metadataRoot / "providers";
    std::filesystem::remove_all(root);
    std::filesystem::remove_all(backupRoot);
    std::filesystem::create_directories(providersRoot);

    const auto makeDescriptor = [](const std::string &name) {
        return nlohmann::json {
            { "type", "hex.builtin.provider.mem_file" },
            { "settings", {
                { "baseAddress", 0 },
                { "currPage", 0 },
                { "data", std::vector<u8> { 0x01 } },
                { "name", name },
                { "readOnly", false }
            } }
        };
    };

    wolv::io::File(metadataRoot / "project.json", wolv::io::File::Mode::Create)
        .writeString(R"({"version":1,"associations":{}})");
    wolv::io::File(providersRoot / "providers.json", wolv::io::File::Mode::Create)
        .writeString(R"({"providers":[41,42,100],"closedProviders":[100]})");
    for (const auto id : { 41, 42 }) {
        wolv::io::File(providersRoot / fmt::format("{}.json", id), wolv::io::File::Mode::Create)
            .writeString(makeDescriptor(fmt::format("Provider {}", id)).dump());
    }
    wolv::io::File(root / "closed.bin", wolv::io::File::Mode::Create).writeVector({ 0xAA });
    wolv::io::File(providersRoot / "100.json", wolv::io::File::Mode::Create).writeString(nlohmann::json {
        { "type", "hex.builtin.provider.file" },
        { "settings", {
            { "baseAddress", 0 },
            { "currPage", 0 },
            { "path", "closed.bin" }
        } }
    }.dump());

    TEST_ASSERT(ProjectManager::load(root));
    auto providers = ImHexApi::Provider::getProviders();
    TEST_ASSERT(providers.size() == 2);
    TEST_ASSERT(std::ranges::none_of(providers, [](const auto *provider) { return provider->getID() == 100; }));

    auto temporaryProvider = ImHexApi::Provider::createProvider("hex.builtin.provider.mem_file"_unlocalized, true);
    TEST_ASSERT(temporaryProvider->getID() > 100);
    ImHexApi::Provider::remove(temporaryProvider.get(), true);

    TEST_ASSERT(ProjectManager::store(backupRoot, false));
    const auto backupClosedSettings = nlohmann::json::parse(
        wolv::io::File(backupRoot / ".imhex/providers/100.json", wolv::io::File::Mode::Read).readString());
    const auto expectedBackupPath = std::filesystem::proximate(root / "closed.bin", backupRoot);
    TEST_ASSERT(std::filesystem::path(backupClosedSettings["settings"]["path"].get<std::string>()) == expectedBackupPath);
    const auto canonicalClosedSettings = nlohmann::json::parse(
        wolv::io::File(providersRoot / "100.json", wolv::io::File::Mode::Read).readString());
    TEST_ASSERT(canonicalClosedSettings["settings"]["path"] == "closed.bin");

    const recent::RecentEntry closedRecentEntry {
        .displayName = "closed.bin",
        .type = "hex.builtin.provider.file",
        .entryFilePath = {},
        .data = {
            { "baseAddress", 0 },
            { "currPage", 0 },
            { "path", (root / "closed.bin").string() }
        }
    };
    recent::loadRecentEntry(closedRecentEntry);
    providers = ImHexApi::Provider::getProviders();
    const auto reopenedProvider = std::ranges::find_if(providers, [](const auto *provider) { return provider->getID() == 100; });
    TEST_ASSERT(reopenedProvider != providers.end());
    TEST_ASSERT(providers.size() == 3);
    recent::loadRecentEntry(closedRecentEntry);
    TEST_ASSERT(ImHexApi::Provider::get()->getID() == 100);
    TEST_ASSERT(ImHexApi::Provider::getProviders().size() == 3);
    ImHexApi::Provider::remove(*reopenedProvider, true);
    providers = ImHexApi::Provider::getProviders();

    const auto providerToClose = *std::ranges::find_if(providers, [](const auto *provider) { return provider->getID() == 42; });
    providerToClose->resize(3);
    ImHexApi::Provider::resetDataDirty();
    ImHexApi::Provider::remove(providerToClose, true);
    TEST_ASSERT(ProjectManager::store());

    auto manifest = nlohmann::json::parse(
        wolv::io::File(providersRoot / "providers.json", wolv::io::File::Mode::Read).readString());
    TEST_ASSERT(manifest["providers"].get<std::set<u32>>() == std::set<u32>({ 41, 42, 100 }));
    TEST_ASSERT(manifest["closedProviders"].get<std::set<u32>>() == std::set<u32>({ 42, 100 }));
    const auto closedProviderSettings = nlohmann::json::parse(
        wolv::io::File(providersRoot / "42.json", wolv::io::File::Mode::Read).readString());
    TEST_ASSERT(closedProviderSettings["settings"]["data"].size() == 3);

    TEST_ASSERT(ProjectManager::load(root));
    providers = ImHexApi::Provider::getProviders();
    TEST_ASSERT(providers.size() == 1);
    TEST_ASSERT(providers.front()->getID() == 41);

    manifest.erase("closedProviders");
    wolv::io::File(providersRoot / "providers.json", wolv::io::File::Mode::Create).writeString(manifest.dump());
    TEST_ASSERT(ProjectManager::load(root));
    providers = ImHexApi::Provider::getProviders();
    TEST_ASSERT(providers.size() == 3);

    project::ImportedProvider unavailableProvider {
        .id = 200,
        .descriptor = {
            { "type", "hex.test.provider.unavailable" },
            { "settings", nlohmann::json::object() }
        },
        .files = {},
        .patches = {},
    };
    const auto importResult = project::importProviders({ std::move(unavailableProvider) });
    TEST_ASSERT(importResult.success);
    TEST_ASSERT(importResult.failedProviderIds == std::vector<u32>({ 200 }));
    auto postImportProvider = ImHexApi::Provider::createProvider("hex.builtin.provider.mem_file"_unlocalized, true);
    TEST_ASSERT(postImportProvider->getID() > 200);

    TEST_SUCCESS();
};
