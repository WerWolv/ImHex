#pragma once

#include <hex.hpp>

#include <list>
#include <map>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include <hex/providers/overlay.hpp>
#include <hex/helpers/fs.hpp>

#include <nlohmann/json_fwd.hpp>

#include <hex/providers/undo_redo/stack.hpp>

namespace hex::prv {

    /**
     * @brief Represent the data source for a tab in the UI
     */
    class Provider {
    public:
        struct Description {
            std::string name;
            std::string value;
        };

        struct MenuEntry {
            std::string name;
            const char *icon;
            std::function<void()> callback;
        };

        constexpr static u64 MaxPageSize = 0xFFFF'FFFF'FFFF'FFFF;

        Provider();
        virtual ~Provider();
        Provider(const Provider&) = delete;
        Provider& operator=(const Provider&) = delete;

        Provider(Provider &&provider) noexcept = default;
        Provider& operator=(Provider &&provider) noexcept = default;

        /**
         * @brief Opens this provider
         * @note The return value of this function allows to ensure the provider is available,
         * so calling Provider::isAvailable() just after a call to open() that returned true is redundant.
         * @note This is not related to the EventProviderOpened event
         * @return true if the provider was opened successfully, else false
         */
        [[nodiscard]] virtual bool open() = 0;

        /**
         * @brief Closes this provider
         * @note This function is called when the user requests for a provider to be closed, e.g. by closing a tab.
         * In general, this function should close the underlying data source but leave the provider in a state where
         * it can be opened again later by calling the open() function again.
         */
        virtual void close() = 0;

        /**
         * @brief Checks if this provider is open and can be used to access data
         * @return Generally, if the open() function succeeded and the data source was successfully opened, this
         * function should return true
         */
        [[nodiscard]] virtual bool isAvailable() const = 0;

        /**
         * @brief Checks if the data in this provider can be read
         * @return True if the provider is readable, false otherwise
         */
        [[nodiscard]] virtual bool isReadable() const  = 0;

        /**
         * @brief Controls if the user can write data to this specific provider.
         *   This may be false for e.g. a file opened in read-only
         */
        [[nodiscard]] virtual bool isWritable() const  = 0;

        /**
         * @brief Controls if the user can resize this provider
         * @return True if the provider is resizable, false otherwise
         */
        [[nodiscard]] virtual bool isResizable() const = 0;

        /**
         * @brief Controls whether the provider can be saved ("saved", not "saved as")
         *   This is mainly used by providers that aren't buffered, and so don't need to be saved
         *   This function will usually return false for providers that aren't writable, but this isn't guaranted
         */
        [[nodiscard]] virtual bool isSavable() const = 0;

        /**
         * @brief Controls whether we can dump data from this provider (e.g. "save as", or "export -> ..").
         *   Typically disabled for process with sparse data, like the Process memory provider
         *   where the virtual address space is several TiBs large.
         *   Default implementation returns true.
         */
        [[nodiscard]] virtual bool isDumpable() const;

        /**
         * @brief Controls whether this provider can be saved as a recent entry
         *   Typically used for providers that do not retain data, e.g. the memory provider
         */
        [[nodiscard]] virtual bool isSavableAsRecent() const { return true; }

        /**
         * @brief Read data from this provider, applying overlays and patches
         * @param offset offset to start reading the data
         * @param buffer buffer to write read data
         * @param size number of bytes to read
         * @param overlays apply overlays and patches is true. Same as readRaw() if false
         */
        void read(u64 offset, void *buffer, size_t size, bool overlays = true);
        
        /**
         * @brief Write data to the patches of this provider. Will not directly modify provider.
         * @param offset offset to start writing the data
         * @param buffer buffer to take data to write from
         * @param size number of bytes to write
         */
        void write(u64 offset, const void *buffer, size_t size);

        /**
         * @brief Read data from this provider, without applying overlays and patches
         * @param offset offset to start reading the data
         * @param buffer buffer to write read data
         * @param size number of bytes to read
         */
        virtual void readRaw(u64 offset, void *buffer, size_t size) = 0;
        /**
         * @brief Write data directly to this provider
         * @param offset offset to start writing the data
         * @param buffer buffer to take data to write from
         * @param size number of bytes to write
         */
        virtual void writeRaw(u64 offset, const void *buffer, size_t size) = 0;

        /**
         * @brief Get the full size of the data in this provider
         * @return The size of the entire available data of this provider
         */
        [[nodiscard]] virtual u64 getActualSize() const = 0;

        /**
         * @brief Gets the type name of this provider
         * @note This is mainly used to be stored in project files and recents to be able to later on
         * recreate this exact provider type. This needs to be unique across all providers, this is usually something
         * like "hex.builtin.provider.mem_file" or "hex.builtin.provider.file"
         * @return The provider's type name
         */
        [[nodiscard]] virtual UnlocalizedString getTypeName() const = 0;

        /**
         * @brief Gets a human readable representation of the current provider
         * @note This is mainly used to display the provider in the UI. For example, the file provider
         * will return the file name here
         * @return The name of the current provider
         */
        [[nodiscard]] virtual std::string getName() const = 0;

        bool resize(u64 newSize);
        void insert(u64 offset, u64 size);
        void remove(u64 offset, u64 size);

        virtual void resizeRaw(u64 newSize) { std::ignore = newSize; }
        virtual void insertRaw(u64 offset, u64 size);
        virtual void removeRaw(u64 offset, u64 size);

        virtual void save();
        virtual void saveAs(const std::fs::path &path);

        [[nodiscard]] Overlay *newOverlay();
        void deleteOverlay(Overlay *overlay);
        void applyOverlays(u64 offset, void *buffer, size_t size) const;
        [[nodiscard]] const std::list<std::unique_ptr<Overlay>> &getOverlays() const;

        [[nodiscard]] u64 getPageSize() const;
        void setPageSize(u64 pageSize);

        [[nodiscard]] u32 getPageCount() const;
        [[nodiscard]] u32 getCurrentPage() const;
        void setCurrentPage(u32 page);

        virtual void setBaseAddress(u64 address);
        [[nodiscard]] virtual u64 getBaseAddress() const;
        [[nodiscard]] virtual u64 getCurrentPageAddress() const;
        [[nodiscard]] virtual u64 getSize() const;
        [[nodiscard]] virtual std::optional<u32> getPageOfAddress(u64 address) const;

        [[nodiscard]] virtual std::vector<Description> getDataDescription() const;
        [[nodiscard]] virtual std::variant<std::string, i128> queryInformation(const std::string &category, const std::string &argument);

        void undo();
        void redo();

        [[nodiscard]] bool canUndo() const;
        [[nodiscard]] bool canRedo() const;

        [[nodiscard]] virtual bool hasFilePicker() const;
        virtual bool handleFilePicker();

        virtual std::vector<MenuEntry> getMenuEntries() { return { }; }

        [[nodiscard]] virtual bool hasLoadInterface() const;
        [[nodiscard]] virtual bool hasInterface() const;
        virtual bool drawLoadInterface();
        virtual void drawInterface();

        [[nodiscard]] u32 getID() const;
        void setID(u32 id);

        [[nodiscard]] virtual nlohmann::json storeSettings(nlohmann::json settings) const;
        virtual void loadSettings(const nlohmann::json &settings);

        void markDirty(bool dirty = true) { m_dirty = dirty; }
        [[nodiscard]] bool isDirty() const { return m_dirty; }

        [[nodiscard]] virtual std::pair<Region, bool> getRegionValidity(u64 address) const;

        void skipLoadInterface() { m_skipLoadInterface = true; }
        [[nodiscard]] bool shouldSkipLoadInterface() const { return m_skipLoadInterface; }

        void setErrorMessage(const std::string &errorMessage) { m_errorMessage = errorMessage; }
        [[nodiscard]] const std::string& getErrorMessage() const { return m_errorMessage; }

        template<std::derived_from<undo::Operation> T>
        bool addUndoableOperation(auto && ... args) {
            return m_undoRedoStack.add<T>(std::forward<decltype(args)...>(args)...);
        }

        [[nodiscard]] undo::Stack& getUndoStack() { return m_undoRedoStack; }

    protected:
        u32 m_currPage    = 0;
        u64 m_baseAddress = 0;

        undo::Stack m_undoRedoStack;

        std::list<std::unique_ptr<Overlay>> m_overlays;

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

        std::string m_errorMessage = "Unspecified error";

        u64 m_pageSize = MaxPageSize;
    };

}