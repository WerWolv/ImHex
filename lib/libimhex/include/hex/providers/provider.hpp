#pragma once

#include <hex.hpp>

#include <list>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include <hex/providers/overlay.hpp>
#include <hex/helpers/fs.hpp>

namespace pl {
    class PatternLanguage;
}

namespace hex::prv {

    class Provider {
    public:
        constexpr static size_t PageSize = 0x1000'0000;

        Provider();
        virtual ~Provider();

        [[nodiscard]] virtual bool isAvailable() const = 0;
        [[nodiscard]] virtual bool isReadable() const  = 0;
        [[nodiscard]] virtual bool isWritable() const  = 0;
        [[nodiscard]] virtual bool isResizable() const = 0;
        [[nodiscard]] virtual bool isSavable() const   = 0;

        virtual void read(u64 offset, void *buffer, size_t size, bool overlays = true);
        virtual void write(u64 offset, const void *buffer, size_t size);

        virtual void resize(size_t newSize);
        virtual void insert(u64 offset, size_t size);

        virtual void save();
        virtual void saveAs(const std::fs::path &path);

        virtual void readRaw(u64 offset, void *buffer, size_t size)        = 0;
        virtual void writeRaw(u64 offset, const void *buffer, size_t size) = 0;
        [[nodiscard]] virtual size_t getActualSize() const                 = 0;

        void applyOverlays(u64 offset, void *buffer, size_t size);

        [[nodiscard]] std::map<u64, u8> &getPatches();
        [[nodiscard]] const std::map<u64, u8> &getPatches() const;
        void applyPatches();

        [[nodiscard]] Overlay *newOverlay();
        void deleteOverlay(Overlay *overlay);
        [[nodiscard]] const std::list<Overlay *> &getOverlays();

        [[nodiscard]] u32 getPageCount() const;
        [[nodiscard]] u32 getCurrentPage() const;
        void setCurrentPage(u32 page);

        virtual void setBaseAddress(u64 address);
        [[nodiscard]] virtual u64 getBaseAddress() const;
        [[nodiscard]] virtual u64 getCurrentPageAddress() const;
        [[nodiscard]] virtual size_t getSize() const;
        [[nodiscard]] virtual std::optional<u32> getPageOfAddress(u64 address) const;

        [[nodiscard]] virtual std::string getName() const                                                 = 0;
        [[nodiscard]] virtual std::vector<std::pair<std::string, std::string>> getDataInformation() const = 0;

        [[nodiscard]] virtual bool open() = 0;
        virtual void close()              = 0;

        void addPatch(u64 offset, const void *buffer, size_t size, bool createUndo = false);
        void createUndoPoint();

        void undo();
        void redo();

        [[nodiscard]] bool canUndo() const;
        [[nodiscard]] bool canRedo() const;

        [[nodiscard]] virtual bool hasLoadInterface() const;
        [[nodiscard]] virtual bool hasInterface() const;
        virtual void drawLoadInterface();
        virtual void drawInterface();

        pl::PatternLanguage &getPatternLanguageRuntime() { return *this->m_patternLanguageRuntime; }
        std::string &getPatternLanguageSourceCode() { return this->m_patternLanguageSourceCode; }

    protected:
        u32 m_currPage    = 0;
        u64 m_baseAddress = 0;

        u32 m_patchTreeOffset = 0;
        std::list<std::map<u64, u8>> m_patches;
        std::list<Overlay *> m_overlays;

        std::unique_ptr<pl::PatternLanguage> m_patternLanguageRuntime;
        std::string m_patternLanguageSourceCode;
    };

}