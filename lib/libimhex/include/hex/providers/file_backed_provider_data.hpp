#pragma once

#include <hex/api/imhex_api/provider.hpp>
#include <hex/api/task_manager.hpp>
#include <hex/api/events/events_provider.hpp>
#include <hex/api/events/events_gui.hpp>
#include <hex/api/events/events_lifecycle.hpp>
#include <hex/api/events/requests_provider.hpp>
#include <hex/helpers/fs.hpp>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <ranges>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <wolv/io/file.hpp>

namespace hex {

    namespace impl {
        bool writeFileAtomically(const std::fs::path &path, std::span<const u8> data);
    }

    #if !defined(HEX_MODULE_EXPORT)
        namespace prv {
            class Provider;
        }
    #endif

    struct FileBackedProviderDataType {
        std::string typeId;
        UnlocalizedString displayName;
        std::string displayIcon;
        std::vector<fs::ItemFilter> extensions;
    };

    class FileBackedProviderDataBase {
    public:
        FileBackedProviderDataBase(const FileBackedProviderDataBase &) = delete;
        FileBackedProviderDataBase(FileBackedProviderDataBase &&) = delete;
        FileBackedProviderDataBase &operator=(const FileBackedProviderDataBase &) = delete;
        FileBackedProviderDataBase &operator=(FileBackedProviderDataBase &&) = delete;
        virtual ~FileBackedProviderDataBase();

        [[nodiscard]] const FileBackedProviderDataType &getType() const { return m_type; }

        [[nodiscard]] virtual bool bind(prv::Provider *provider, const std::fs::path &path) = 0;
        [[nodiscard]] virtual bool createFile(const std::fs::path &path) const = 0;
        [[nodiscard]] virtual bool relocate(prv::Provider *provider, const std::fs::path &path) = 0;
        virtual void unbind(prv::Provider *provider) = 0;
        [[nodiscard]] virtual bool isBound(const prv::Provider *provider) const = 0;
        [[nodiscard]] virtual std::optional<std::fs::path> getBinding(const prv::Provider *provider) const = 0;
        [[nodiscard]] virtual bool hasPendingData(const prv::Provider *provider) const = 0;
        virtual void synchronize() = 0;
        [[nodiscard]] virtual bool flush() = 0;

    protected:
        explicit FileBackedProviderDataBase(FileBackedProviderDataType type);

    private:
        FileBackedProviderDataType m_type;
    };

    /**
     * @brief Signals that file-backed provider data or its persistence state changed
     */
    EVENT_DEF(EventFileBackedProviderDataChanged, prv::Provider *, FileBackedProviderDataBase *);

    class FileBackedProviderDataRegistry {
    public:
        [[nodiscard]] static std::vector<FileBackedProviderDataBase *> getTypes();
        [[nodiscard]] static FileBackedProviderDataBase *get(std::string_view typeId);

        [[nodiscard]] static bool bind(prv::Provider *provider, std::string_view typeId, const std::fs::path &path);
        [[nodiscard]] static bool createFile(std::string_view typeId, const std::fs::path &path);
        [[nodiscard]] static bool relocate(prv::Provider *provider, std::string_view typeId, const std::fs::path &path);
        [[nodiscard]] static bool unbind(prv::Provider *provider, std::string_view typeId);
        [[nodiscard]] static bool isBound(const prv::Provider *provider, std::string_view typeId);
        [[nodiscard]] static std::optional<std::fs::path> getBinding(const prv::Provider *provider, std::string_view typeId);

    private:
        friend class FileBackedProviderDataBase;
        template<typename> friend class FileBackedProviderData;

        static void add(FileBackedProviderDataBase *data);
        static void remove(FileBackedProviderDataBase *data) noexcept;
    };

    template<typename T>
    class FileBackedProviderData : public FileBackedProviderDataBase {
    public:
        using SerializedData = std::vector<u8>;
        using Clock = std::chrono::steady_clock;

        struct Descriptor {
            std::string typeId;
            UnlocalizedString displayName;
            std::string displayIcon;
            std::vector<fs::ItemFilter> extensions;
            std::function<SerializedData(const T &)> encode;
            std::function<std::optional<T>(std::span<const u8>)> decode;
            std::function<T()> createDefault = [] { return T(); };
            std::chrono::milliseconds debounce = std::chrono::milliseconds(10000ll);
        };

        explicit FileBackedProviderData(Descriptor descriptor)
            : FileBackedProviderDataBase({
                descriptor.typeId,
                descriptor.displayName,
                descriptor.displayIcon,
                descriptor.extensions
            }), m_descriptor(std::move(descriptor)) {
            if (!m_descriptor.encode || !m_descriptor.decode || !m_descriptor.createDefault)
                throw std::invalid_argument("FileBackedProviderData requires encode, decode and createDefault callbacks");

            EventProviderOpened::subscribe(this, [this](prv::Provider *provider) {
                std::ignore = this->getOrCreate(provider);
            });

            EventProviderDeleted::subscribe(this, [this](prv::Provider *provider) {
                m_entries.erase(provider);
            });

            EventImHexClosing::subscribe(this, [this] {
                m_entries.clear();
            });

            MovePerProviderData::subscribe(this, [this](prv::Provider *from, prv::Provider *to) {
                this->finishSynchronization();
                auto node = m_entries.extract(from);
                if (node.empty())
                    return;

                m_entries.erase(to);
                node.key() = to;
                m_entries.insert(std::move(node));
            });

            EventFrameEnd::subscribe(this, [this] {
                this->synchronize();
            });
        }

        ~FileBackedProviderData() override {
            EventFrameEnd::unsubscribe(this);
            MovePerProviderData::unsubscribe(this);
            EventImHexClosing::unsubscribe(this);
            EventProviderDeleted::unsubscribe(this);
            EventProviderOpened::unsubscribe(this);
            m_synchronizationTask.wait();
        }

        T *operator->() { return &this->get(); }
        const T *operator->() const { return &this->get(); }

        T &get(const prv::Provider *provider = ImHexApi::Provider::get()) {
            return this->getOrCreate(provider).value;
        }

        const T &get(const prv::Provider *provider = ImHexApi::Provider::get()) const {
            return const_cast<FileBackedProviderData *>(this)->getOrCreate(provider).value;
        }

        void set(const T &data, const prv::Provider *provider = ImHexApi::Provider::get()) {
            auto &entry = this->getOrCreate(provider);
            entry.value = data;
            this->markChanged(provider);
        }

        void set(T &&data, const prv::Provider *provider = ImHexApi::Provider::get()) {
            auto &entry = this->getOrCreate(provider);
            entry.value = std::move(data);
            this->markChanged(provider);
        }

        T &operator*() { return this->get(); }
        const T &operator*() const { return this->get(); }

        FileBackedProviderData &operator=(const T &data) {
            this->set(data);
            return *this;
        }

        FileBackedProviderData &operator=(T &&data) {
            this->set(std::move(data));
            return *this;
        }

        operator T&() { return this->get(); }

        void setChangedCallback(std::function<void(prv::Provider *)> callback) {
            m_changedCallback = std::move(callback);
        }

        /**
         * @brief Signal an in-place mutation made through get(), operator*(), or operator->().
         */
        void markChanged(const prv::Provider *provider = ImHexApi::Provider::get()) {
            auto &entry = this->getOrCreate(provider);
            entry.revision += 1;
            entry.touched = true;
            if (entry.path.has_value()) {
                entry.dirtySince = Clock::now();
                const auto deadline = *entry.dirtySince + m_descriptor.debounce;
                if (!m_nextSaveTime.has_value() || deadline < *m_nextSaveTime)
                    m_nextSaveTime = deadline;
            }

            this->notify(provider);
        }

        auto all() {
            return m_entries | std::views::values | std::views::transform([](auto &entry) -> T& {
                return entry->value;
            });
        }

        auto all() const {
            return m_entries | std::views::values | std::views::transform([](const auto &entry) -> const T& {
                return entry->value;
            });
        }

        [[nodiscard]] bool bind(prv::Provider *provider, const std::fs::path &path) override {
            this->validateProvider(provider);
            if (path.empty())
                return false;
            this->finishSynchronization();

            auto &entry = this->getOrCreate(provider);
            std::error_code error;
            const bool fileExists = std::fs::exists(path, error);
            if (error)
                return false;

            SerializedData serialized;
            std::optional<T> decoded;
            if (fileExists) {
                auto contents = readFile(path);
                if (!contents.has_value())
                    return false;

                serialized = std::move(*contents);
                decoded = m_descriptor.decode(std::span<const u8>(serialized));
                if (!decoded.has_value())
                    return false;
            } else {
                serialized = m_descriptor.encode(entry.value);
            }

            entry.tracker.stopTracking();
            if (decoded.has_value())
                entry.value = std::move(*decoded);
            entry.serialized = serialized;
            entry.diskSerialized = fileExists ? std::move(serialized) : SerializedData {};
            entry.dirtySince.reset();
            if (!fileExists)
                entry.dirtySince = Clock::now() - m_descriptor.debounce;
            entry.touched = !fileExists;
            entry.missing = false;
            entry.reloadPending.store(false, std::memory_order_release);
            entry.path = path;
            entry.missing = false;
            entry.tracker = wolv::io::ChangeTracker(path);
            entry.tracker.startTracking([this, pending = &entry.reloadPending, revision = &entry.reloadRevision] {
                revision->fetch_add(1, std::memory_order_relaxed);
                pending->store(true, std::memory_order_release);
                m_externalWorkPending.store(true, std::memory_order_release);
            });
            if (!fileExists)
                m_nextSaveTime = Clock::now();

            this->notify(provider);
            return true;
        }

        [[nodiscard]] bool createFile(const std::fs::path &path) const override {
            std::error_code error;
            if (path.empty() || std::fs::exists(path, error) || error)
                return false;

            return writeFile(path, m_descriptor.encode(m_descriptor.createDefault()));
        }

        [[nodiscard]] bool relocate(prv::Provider *provider, const std::fs::path &path) override {
            this->validateProvider(provider);
            this->finishSynchronization();
            const auto it = m_entries.find(provider);
            if (it == m_entries.end() || !it->second->path.has_value() || path.empty())
                return false;

            auto &entry = *it->second;
            entry.tracker.stopTracking();
            entry.reloadPending.store(false, std::memory_order_release);
            entry.path = path;
            entry.missing = false;
            entry.tracker = wolv::io::ChangeTracker(path);
            entry.tracker.startTracking([this, pending = &entry.reloadPending, revision = &entry.reloadRevision] {
                revision->fetch_add(1, std::memory_order_relaxed);
                pending->store(true, std::memory_order_release);
                m_externalWorkPending.store(true, std::memory_order_release);
            });
            return true;
        }

        void unbind(prv::Provider *provider) override {
            this->validateProvider(provider);
            this->finishSynchronization();
            auto defaultValue = m_descriptor.createDefault();
            auto serialized = m_descriptor.encode(defaultValue);
            auto &entry = this->getOrCreate(provider);

            entry.tracker.stopTracking();
            entry.tracker = wolv::io::ChangeTracker();
            entry.path.reset();
            entry.value = std::move(defaultValue);
            entry.serialized = std::move(serialized);
            entry.diskSerialized.clear();
            entry.dirtySince.reset();
            entry.touched = false;
            entry.reloadPending.store(false, std::memory_order_release);
            this->notify(provider);
        }

        [[nodiscard]] bool isBound(const prv::Provider *provider) const override {
            this->validateProvider(provider);
            const auto it = m_entries.find(provider);
            return it != m_entries.end() && it->second->path.has_value();
        }

        [[nodiscard]] std::optional<std::fs::path> getBinding(const prv::Provider *provider) const override {
            this->validateProvider(provider);
            const auto it = m_entries.find(provider);
            if (it == m_entries.end())
                return std::nullopt;

            return it->second->path;
        }

        [[nodiscard]] bool hasPendingData(const prv::Provider *provider) const override {
            this->validateProvider(provider);
            const auto it = m_entries.find(provider);
            return it != m_entries.end() && it->second->touched;
        }

        void synchronize() override {
            this->applySynchronizationResults();
            if (m_synchronizationTask.isRunning())
                return;

            const auto now = Clock::now();
            const bool externalWorkPending = m_externalWorkPending.exchange(false, std::memory_order_acq_rel);
            if (!externalWorkPending && (!m_nextSaveTime.has_value() || now < *m_nextSaveTime))
                return;

            std::vector<SynchronizationOperation> operations;
            m_nextSaveTime.reset();

            for (auto &[provider, entryPtr] : m_entries) {
                auto &entry = *entryPtr;

                if (entry.path.has_value() && !entry.dirtySince.has_value() &&
                    entry.reloadPending.exchange(false, std::memory_order_acq_rel)) {
                    operations.push_back({
                        .type = SynchronizationOperation::Type::Read,
                        .provider = provider,
                        .path = *entry.path,
                        .revision = entry.revision,
                        .reloadRevision = entry.reloadRevision.load(std::memory_order_relaxed),
                        .data = {}
                    });
                } else if (entry.path.has_value() && entry.dirtySince.has_value() &&
                           now - *entry.dirtySince >= m_descriptor.debounce) {
                    operations.push_back({
                        .type = SynchronizationOperation::Type::Write,
                        .provider = provider,
                        .path = *entry.path,
                        .revision = entry.revision,
                        .reloadRevision = 0,
                        .data = m_descriptor.encode(entry.value)
                    });
                }

                if (entry.dirtySince.has_value()) {
                    const auto deadline = *entry.dirtySince + m_descriptor.debounce;
                    if (!m_nextSaveTime.has_value() || deadline < *m_nextSaveTime)
                        m_nextSaveTime = deadline;
                }

            }

            if (operations.empty())
                return;

            m_synchronizationTask = TaskManager::createBackgroundTask(
                "Synchronizing " + this->getType().typeId,
                [operations = std::move(operations), results = &m_synchronizationResults,
                 resultsMutex = &m_synchronizationResultsMutex] mutable {
                    for (auto &operation : operations) {
                        if (operation.type == SynchronizationOperation::Type::Read) {
                            auto data = readFile(operation.path);
                            operation.success = data.has_value();
                            if (data.has_value())
                                operation.data = std::move(*data);
                        } else {
                            operation.success = writeFile(operation.path, operation.data);
                        }
                    }
                    std::scoped_lock lock(*resultsMutex);
                    *results = std::move(operations);
                });
        }

        [[nodiscard]] bool flush() override {
            this->finishSynchronization();
            bool result = true;
            for (auto &[provider, entryPtr] : m_entries) {
                if (!entryPtr->path.has_value() || entryPtr->missing || !entryPtr->touched)
                    continue;
                result = this->flush(provider) && result;
            }
            return result;
        }

        [[nodiscard]] bool flush(const prv::Provider *provider) {
            this->validateProvider(provider);
            this->finishSynchronization();
            const auto it = m_entries.find(provider);
            if (it == m_entries.end() || !it->second->path.has_value() ||
                it->second->missing || !it->second->touched)
                return false;

            auto &entry = *it->second;
            entry.serialized = m_descriptor.encode(entry.value);
            if (!writeFile(*entry.path, entry.serialized))
                return false;

            entry.diskSerialized = entry.serialized;
            entry.dirtySince.reset();
            entry.touched = false;
            entry.missing = false;
            this->notifyStateChanged(provider);
            return true;
        }

    private:
        struct Entry {
            Entry(T initialValue, SerializedData initialSerialized)
                : value(std::move(initialValue)), serialized(std::move(initialSerialized)) { }

            T value;
            std::optional<std::fs::path> path;
            wolv::io::ChangeTracker tracker;
            std::atomic_bool reloadPending = false;
            std::atomic<u64> reloadRevision = 0;
            SerializedData serialized;
            SerializedData diskSerialized;
            std::optional<Clock::time_point> dirtySince;
            bool missing = false;
            bool touched = false;
            u64 revision = 0;
        };

        struct SynchronizationOperation {
            enum class Type : u8 {
                Read,
                Write
            };

            Type type;
            const prv::Provider *provider;
            std::fs::path path;
            u64 revision;
            u64 reloadRevision;
            SerializedData data;
            bool success = false;
        };

        static void validateProvider(const prv::Provider *provider) {
            if (provider == nullptr) [[unlikely]]
                throw std::invalid_argument("FileBackedProviderData called with nullptr provider");
        }

        Entry &getOrCreate(const prv::Provider *provider) {
            this->validateProvider(provider);
            if (auto it = m_entries.find(provider); it != m_entries.end())
                return *it->second;

            auto value = m_descriptor.createDefault();
            auto serialized = m_descriptor.encode(value);
            auto [it, inserted] = m_entries.emplace(provider, std::make_unique<Entry>(std::move(value), std::move(serialized)));
            std::ignore = inserted;
            return *it->second;
        }

        static std::optional<SerializedData> readFile(const std::fs::path &path) {
            wolv::io::File file(path, wolv::io::File::Mode::Read);
            if (!file.isValid())
                return std::nullopt;

            return file.readVector();
        }

        static bool writeFile(const std::fs::path &path, const SerializedData &data) {
            return impl::writeFileAtomically(path, data);
        }

        void finishSynchronization() {
            m_synchronizationTask.wait();
            this->applySynchronizationResults();
        }

        void applySynchronizationResults() {
            if (m_synchronizationTask.isRunning())
                return;

            std::vector<SynchronizationOperation> results;
            {
                std::scoped_lock lock(m_synchronizationResultsMutex);
                results.swap(m_synchronizationResults);
            }
            if (results.empty())
                return;

            std::vector<const prv::Provider *> changedProviders;
            std::vector<const prv::Provider *> settledProviders;
            for (auto &result : results) {
                const auto it = m_entries.find(result.provider);
                if (it == m_entries.end())
                    continue;

                auto &entry = *it->second;
                if (!entry.path.has_value() || *entry.path != result.path || entry.revision != result.revision)
                    continue;

                if (result.type == SynchronizationOperation::Type::Write) {
                    if (result.success) {
                        entry.serialized = std::move(result.data);
                        entry.diskSerialized = entry.serialized;
                        entry.dirtySince.reset();
                        entry.touched = false;
                        entry.missing = false;
                        settledProviders.push_back(result.provider);
                    } else {
                        entry.dirtySince = Clock::now();
                    }
                    continue;
                }

                if (entry.dirtySince.has_value() ||
                    entry.reloadRevision.load(std::memory_order_relaxed) != result.reloadRevision) {
                    entry.reloadPending.store(true, std::memory_order_release);
                    m_externalWorkPending.store(true, std::memory_order_release);
                    continue;
                }

                if (!result.success) {
                    if (!entry.missing) {
                        entry.value = m_descriptor.createDefault();
                        entry.serialized = m_descriptor.encode(entry.value);
                        entry.diskSerialized.clear();
                        entry.dirtySince.reset();
                        entry.touched = false;
                        entry.missing = true;
                        changedProviders.push_back(result.provider);
                    }
                } else if (result.data != entry.diskSerialized) {
                    auto decoded = m_descriptor.decode(std::span<const u8>(result.data));
                    if (decoded.has_value()) {
                        entry.value = std::move(*decoded);
                        entry.serialized = result.data;
                        entry.dirtySince.reset();
                        entry.touched = false;
                        entry.missing = false;
                        changedProviders.push_back(result.provider);
                    }
                    entry.diskSerialized = std::move(result.data);
                }
            }

            for (auto *provider : changedProviders)
                this->notify(provider);
            for (auto *provider : settledProviders)
                this->notifyStateChanged(provider);
        }

        void notify(const prv::Provider *provider) const {
            auto *mutableProvider = const_cast<prv::Provider *>(provider);
            if (m_changedCallback)
                m_changedCallback(mutableProvider);
            EventFileBackedProviderDataChanged::post(mutableProvider, const_cast<FileBackedProviderData *>(this));
        }

        void notifyStateChanged(const prv::Provider *provider) const {
            EventFileBackedProviderDataChanged::post(
                const_cast<prv::Provider *>(provider),
                const_cast<FileBackedProviderData *>(this));
        }

    private:
        Descriptor m_descriptor;
        std::function<void(prv::Provider *)> m_changedCallback;
        std::atomic_bool m_externalWorkPending = false;
        std::optional<Clock::time_point> m_nextSaveTime;
        TaskHolder m_synchronizationTask;
        std::mutex m_synchronizationResultsMutex;
        std::vector<SynchronizationOperation> m_synchronizationResults;
        mutable std::map<const prv::Provider *, std::unique_ptr<Entry>> m_entries;
    };

}
