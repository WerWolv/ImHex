#pragma once

#include <list>
#include <string>
#include <string_view>

#include "patches.hpp"
#include "utils.hpp"

#ifdef __APPLE__
#define off64_t off_t
#define fopen64 fopen
#define fseeko64 fseek
#define ftello64 ftell
#endif

namespace hex {

    class ProjectFile {
    public:
        ProjectFile() = delete;

        static bool load(std::string_view filePath);
        static bool store(std::string_view filePath = "");

        [[nodiscard]] static bool hasUnsavedChanges()       { return ProjectFile::s_hasUnsavedChanged; }
        static void markDirty()                             { if (!ProjectFile::s_currProjectFilePath.empty()) ProjectFile::s_hasUnsavedChanged = true; }

        [[nodiscard]] static std::string getProjectFilePath() { return ProjectFile::s_currProjectFilePath; }

        [[nodiscard]] static std::string getFilePath()      { return ProjectFile::s_filePath; }
        static void setFilePath(std::string_view filePath)  { ProjectFile::s_hasUnsavedChanged = true; ProjectFile::s_filePath = filePath; }

        [[nodiscard]] static std::string getPattern()       { return ProjectFile::s_pattern; }
        static void setPattern(std::string_view pattern)    { ProjectFile::s_hasUnsavedChanged = true; ProjectFile::s_pattern = pattern; }

        [[nodiscard]] static const Patches& getPatches()    { return ProjectFile::s_patches; }
        static void setPatches(const Patches &patches)      { ProjectFile::s_hasUnsavedChanged = true; ProjectFile::s_patches = patches; }

        [[nodiscard]] static const std::list<Bookmark>& getBookmarks()  { return ProjectFile::s_bookmarks; }
        static void setBookmarks(const std::list<Bookmark> &bookmarks)  { ProjectFile::s_hasUnsavedChanged = true; ProjectFile::s_bookmarks = bookmarks; }

    private:
        static inline std::string s_currProjectFilePath;
        static inline bool s_hasUnsavedChanged = false;

        static inline std::string s_filePath;
        static inline std::string s_pattern;
        static inline Patches s_patches;
        static inline std::list<Bookmark> s_bookmarks;
    };

}