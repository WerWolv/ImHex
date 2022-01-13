#pragma once

#include <list>
#include <string>
#include <string_view>

#include "patches.hpp"
#include <hex/api/imhex_api.hpp>
#include <hex/api/event.hpp>

#include <hex/helpers/paths.hpp>

namespace hex {

    class ProjectFile {
    public:
        ProjectFile() = delete;

        static bool load(const std::string &filePath);
        static bool store(std::string filePath = "");

        [[nodiscard]] static bool hasUnsavedChanges() {
            return ProjectFile::s_hasUnsavedChanged;
        }

        static void markDirty() {
            bool setWindowTitle = !hasUnsavedChanges();

            ProjectFile::s_hasUnsavedChanged = true;

            if (setWindowTitle)
                EventManager::post<RequestChangeWindowTitle>(fs::path(getFilePath()).filename().string());
        }

        [[nodiscard]] static const std::string& getProjectFilePath() {
            return ProjectFile::s_currProjectFilePath;
        }

        static void clearProjectFilePath() {
            ProjectFile::s_currProjectFilePath.clear();
        }


        [[nodiscard]] static const std::string& getFilePath() {
            return ProjectFile::s_filePath;
        }

        static void setFilePath(const std::string &filePath) {
            ProjectFile::s_filePath = filePath;

            EventManager::post<RequestChangeWindowTitle>(fs::path(filePath).filename().string());
        }


        [[nodiscard]] static const std::string& getPattern() {
            return ProjectFile::s_pattern;
        }

        static void setPattern(const std::string &pattern) {
            markDirty();
            ProjectFile::s_pattern = pattern;
        }


        [[nodiscard]] static const Patches& getPatches() {
            return ProjectFile::s_patches;
        }

        static void setPatches(const Patches &patches) {
            markDirty();
            ProjectFile::s_patches = patches;
        }


        [[nodiscard]] static const std::list<ImHexApi::Bookmarks::Entry>& getBookmarks() {
            return ProjectFile::s_bookmarks;
        }

        static void setBookmarks(const std::list<ImHexApi::Bookmarks::Entry> &bookmarks) {
            markDirty();
            ProjectFile::s_bookmarks = bookmarks;
        }


        [[nodiscard]] static const std::string& getDataProcessorContent() {
            return ProjectFile::s_dataProcessorContent;
        }

        static void setDataProcessorContent(const std::string &json) {
            markDirty();
            ProjectFile::s_dataProcessorContent = json;
        }

    private:
        static std::string s_currProjectFilePath;
        static bool s_hasUnsavedChanged;

        static std::string s_filePath;
        static std::string s_pattern;
        static Patches s_patches;
        static std::list<ImHexApi::Bookmarks::Entry> s_bookmarks;
        static std::string s_dataProcessorContent;
    };

}