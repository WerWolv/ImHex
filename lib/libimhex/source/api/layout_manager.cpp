#include <hex/api/layout_manager.hpp>

#include <hex/helpers/fs.hpp>
#include <wolv/utils/string.hpp>

#include <imgui.h>

#include <fmt/format.h>

namespace hex {

    std::optional<std::fs::path> LayoutManager::s_layoutPathToLoad;
    std::optional<std::string> LayoutManager::s_layoutStringToLoad;
    std::vector<LayoutManager::Layout> LayoutManager::s_layouts;

    void LayoutManager::load(const std::fs::path &path) {
        s_layoutPathToLoad = path;
    }

    void LayoutManager::loadString(const std::string &content) {
        s_layoutStringToLoad = content;
    }

    void LayoutManager::save(const std::string &name) {
        auto fileName = name;
        fileName = wolv::util::replaceStrings(fileName, " ", "_");
        std::transform(fileName.begin(), fileName.end(), fileName.begin(), tolower);
        fileName += ".hexlyt";

        const auto path = hex::fs::getDefaultPaths(fs::ImHexPath::Layouts).front() / fileName;
        ImGui::SaveIniSettingsToDisk(wolv::util::toUTF8String(path).c_str());

        LayoutManager::reload();
    }

    std::vector<LayoutManager::Layout> LayoutManager::getLayouts() {
        return s_layouts;
    }

    void LayoutManager::process() {
        if (s_layoutPathToLoad.has_value()) {
            ImGui::LoadIniSettingsFromDisk(wolv::util::toUTF8String(*s_layoutPathToLoad).c_str());
            s_layoutPathToLoad = std::nullopt;
        }

        if (s_layoutStringToLoad.has_value()) {
            ImGui::LoadIniSettingsFromMemory(s_layoutStringToLoad->c_str());
            s_layoutStringToLoad = std::nullopt;
        }
    }

    void LayoutManager::reload() {
        s_layouts.clear();

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

                s_layouts.push_back({
                    name,
                    path
                });
            }
        }
    }

    void LayoutManager::reset() {
        s_layoutPathToLoad.reset();
        s_layoutStringToLoad.reset();
        s_layouts.clear();
    }

}