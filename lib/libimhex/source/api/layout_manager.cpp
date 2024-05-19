#include <hex/api/layout_manager.hpp>

#include <hex/helpers/fs.hpp>
#include <hex/helpers/logger.hpp>
#include <hex/helpers/auto_reset.hpp>
#include <wolv/utils/string.hpp>

#include <imgui.h>
#include <hex/api/content_registry.hpp>
#include <hex/ui/view.hpp>

namespace hex {

    namespace {

        AutoReset<std::optional<std::fs::path>> s_layoutPathToLoad;
        AutoReset<std::optional<std::string>> s_layoutStringToLoad;
        AutoReset<std::vector<LayoutManager::Layout>> s_layouts;

        AutoReset<std::vector<LayoutManager::LoadCallback>>  s_loadCallbacks;
        AutoReset<std::vector<LayoutManager::StoreCallback>> s_storeCallbacks;

        bool s_layoutLocked = false;

    }


    void LayoutManager::load(const std::fs::path &path) {
        s_layoutPathToLoad = path;
    }

    void LayoutManager::loadFromString(const std::string &content) {
        s_layoutStringToLoad = content;
    }

    void LayoutManager::save(const std::string &name) {
        auto fileName = name;
        fileName = wolv::util::replaceStrings(fileName, " ", "_");
        std::ranges::transform(fileName, fileName.begin(), tolower);
        fileName += ".hexlyt";

        std::fs::path layoutPath;
        for (const auto &path : hex::fs::getDefaultPaths(fs::ImHexPath::Layouts)) {
            if (!hex::fs::isPathWritable(layoutPath))
                continue;

            layoutPath = path / fileName;
        }

        if (layoutPath.empty()) {
            log::error("Failed to save layout '{}'. No writable path found", name);
            return;
        }

        const auto pathString = wolv::util::toUTF8String(layoutPath);
        ImGui::SaveIniSettingsToDisk(pathString.c_str());
        log::info("Layout '{}' saved to {}", name, pathString);

        LayoutManager::reload();
    }

    std::string LayoutManager::saveToString() {
        return ImGui::SaveIniSettingsToMemory();
    }


    const std::vector<LayoutManager::Layout>& LayoutManager::getLayouts() {
        return s_layouts;
    }

    void LayoutManager::removeLayout(const std::string& name) {
        for (const auto &layout : *s_layouts) {
            if (layout.name == name) {
                if (wolv::io::File(layout.path, wolv::io::File::Mode::Write).remove()) {
                    log::info("Removed layout '{}'", name);
                    LayoutManager::reload();
                } else {
                    log::error("Failed to remove layout '{}'", name);
                }
                return;
            }
        }
    }


    void LayoutManager::closeAllViews() {
        for (const auto &[name, view] : ContentRegistry::Views::impl::getEntries())
            view->getWindowOpenState() = false;
    }

    void LayoutManager::process() {
        if (s_layoutPathToLoad->has_value()) {
            const auto pathString = wolv::util::toUTF8String(**s_layoutPathToLoad);

            LayoutManager::closeAllViews();
            ImGui::LoadIniSettingsFromDisk(pathString.c_str());

            s_layoutPathToLoad = std::nullopt;
            log::info("Loaded layout from {}", pathString);
        }

        if (s_layoutStringToLoad->has_value()) {
            LayoutManager::closeAllViews();
            ImGui::LoadIniSettingsFromMemory((*s_layoutStringToLoad)->c_str());

            s_layoutStringToLoad = std::nullopt;
            log::info("Loaded layout from string");
        }
    }

    void LayoutManager::reload() {
        s_layouts->clear();

        for (const auto &directory : hex::fs::getDefaultPaths(fs::ImHexPath::Layouts)) {
            for (const auto &entry : std::fs::directory_iterator(directory)) {
                const auto &path = entry.path();

                if (path.extension() != ".hexlyt")
                    continue;

                auto name = path.stem().string();
                name = wolv::util::replaceStrings(name, "_", " ");
                for (size_t i = 0; i < name.size(); i++) {
                    if (i == 0 || name[i - 1] == '_')
                        name[i] = char(std::toupper(name[i]));
                }

                s_layouts->push_back({
                    name,
                    path
                });
            }
        }
    }

    void LayoutManager::reset() {
        s_layoutPathToLoad->reset();
        s_layoutStringToLoad->reset();
        s_layouts->clear();
    }

    bool LayoutManager::isLayoutLocked() {
        return s_layoutLocked;
    }

    void LayoutManager::lockLayout(bool locked) {
        log::info("Layout {}", locked ? "locked" : "unlocked");
        s_layoutLocked = locked;
    }

    void LayoutManager::registerLoadCallback(const LoadCallback &callback) {
        s_loadCallbacks->push_back(callback);
    }

    void LayoutManager::registerStoreCallback(const StoreCallback &callback) {
        s_storeCallbacks->push_back(callback);
    }

    void LayoutManager::onLoad(std::string_view line) {
        for (const auto &callback : *s_loadCallbacks)
            callback(line);
    }

    void LayoutManager::onStore(ImGuiTextBuffer *buffer) {
        for (const auto &callback : *s_storeCallbacks)
            callback(buffer);
    }



}
