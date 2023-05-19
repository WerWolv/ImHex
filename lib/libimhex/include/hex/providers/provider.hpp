#pragma once

#include <hex.hpp>

#include <list>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include <hex/api/imhex_api.hpp>
#include <hex/providers/overlay.hpp>
#include <hex/helpers/fs.hpp>

#include <nlohmann/json.hpp>

namespace hex::prv {

    /**
     * @brief Represent the data source for a tab in the UI
     */
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

        /**
         * @brief Read data from this provider, applying overlays and patches
         * @param offset offset to start reading the data
         * @param buffer buffer to write read data
         * @param size number of bytes to read
         * @param overlays apply overlays and patches is true. Same as readRaw() if false
         */
        virtual void read(u64 offset, void *buffer, size_t size, bool overlays = true);
        
        /**
         * @brief Write data to the patches of this provider. Will not directly modify provider.
         * @param offset offset to start writing the data
         * @param buffer buffer to take data to write from
         * @param size number of bytes to write
         */
        virtual void write(u64 offset, const void *buffer, size_t size);

        virtual void resize(size_t newSize);
        virtual void insert(u64 offset, size_t size);
        virtual void remove(u64 offset, size_t size);

        virtual void save();
        virtual void saveAs(const std::fs::path &path);

        /**
         * @brief Read data from this provider, without applying overlays and patches
         * @param offset offset to start reading the data
         * @param buffer buffer to write read data
         * @param size number of bytes to read
         */
        virtual void readRaw(u64 offset, void *buffer, size_t size)        = 0;
        /**
         * @brief Write data directly to this provider
         * @param offset offset to start writing the data
         * @param buffer buffer to take data to write from
         * @param size number of bytes to write
         */
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
        [[nodiscard]] virtual std::vector<std::pair<std::string, std::string>> getDataDescription() const = 0;
        [[nodiscard]] virtual std::variant<std::string, i128> queryInformation(const std::string &category, const std::string &argument);

        /**
         * @brief Opens this provider
         * the return value of this function allows to ensure the provider is available,
         * so calling Provider::isAvailable() just after a call to open() that returned true is dedundant.
         * @note This is not related to the EventProviderOpened event
         * @return true if the provider was opened sucessfully, else false
         */
        [[nodiscard]] virtual bool open() = 0;
        virtual void close() = 0;

        void addPatch(u64 offset, const void *buffer, size_t size, bool createUndo = false);
        void createUndoPoint();

        void undo();
        void redo();

        [[nodiscard]] bool canUndo() const;
        [[nodiscard]] bool canRedo() const;

        [[nodiscard]] virtual bool hasFilePicker() const;
        virtual bool handleFilePicker();

        virtual std::vector<std::pair<std::string, std::function<void()>>> getMenuEntries(){
            return {};
        };

        [[nodiscard]] virtual bool hasLoadInterface() const;
        [[nodiscard]] virtual bool hasInterface() const;
        virtual bool drawLoadInterface();
        virtual void drawInterface();

        [[nodiscard]] u32 getID() const;
        void setID(u32 id);

        [[nodiscard]] virtual nlohmann::json storeSettings(nlohmann::json settings = { }) const;
        virtual void loadSettings(const nlohmann::json &settings);

        [[nodiscard]] virtual std::string getTypeName() const = 0;

        void markDirty(bool dirty = true) { this->m_dirty = dirty; }
        [[nodiscard]] bool isDirty() const { return this->m_dirty; }

        [[nodiscard]] virtual std::pair<Region, bool> getRegionValidity(u64 address) const;

        void skipLoadInterface() { this->m_skipLoadInterface = true; }
        [[nodiscard]] bool shouldSkipLoadInterface() const { return this->m_skipLoadInterface; }

        void setErrorMessage(const std::string &errorMessage) { this->m_errorMessage = errorMessage; }
        [[nodiscard]] const std::string& getErrorMessage() const { return this->m_errorMessage; }

    protected:
        u32 m_currPage    = 0;
        u64 m_baseAddress = 0;

        std::list<std::map<u64, u8>> m_patches;
        decltype(m_patches)::iterator m_currPatches;
        std::list<Overlay *> m_overlays;

        u32 m_id;

        /**
         * @brief true if there is any data that needs to be saved
         */
        bool m_dirty = false;

        /**
         * @brief Control whetever to skip provider initialization
         * initialization may be asking the user for information related to the provider,
         * e.g. a process ID for the process memory provider
         * this is used mainly when restoring a provider with already known initialization information
         * for example when loading a project or loading a provider from the "recent" lsit
         * 
         */
        bool m_skipLoadInterface = false;

        std::string m_errorMessage;

    private:
        static u32 s_idCounter;
    };

}