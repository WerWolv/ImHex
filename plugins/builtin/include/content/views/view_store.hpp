#pragma once

#include <hex.hpp>

#include <hex/ui/view.hpp>

#include <hex/api/task_manager.hpp>
#include <hex/helpers/http_requests.hpp>
#include <hex/helpers/fs.hpp>
#include <hex/helpers/default_paths.hpp>

#include <future>
#include <string>
#include <filesystem>

namespace hex::plugin::builtin {

    enum class RequestStatus {
        NotAttempted,
        InProgress,
        Failed,
        Succeeded,
    };

    struct StoreEntry {
        std::string name;
        std::string description;
        std::vector<std::string> authors;
        std::string fileName;
        std::string link;
        std::string hash;

        bool isFolder;

        bool downloading;
        bool installed;
        bool hasUpdate;
        bool system;
    };

    struct StoreCategory {
        UnlocalizedString unlocalizedName;
        std::string requestName;
        const paths::impl::DefaultPath* path;
        std::vector<StoreEntry> entries;
        std::function<void()> downloadCallback;
    };

    class ViewStore : public View::Floating {
    public:
        ViewStore();
        ~ViewStore() override = default;

        void drawContent() override;

        [[nodiscard]] bool shouldDraw() const override { return true; }
        [[nodiscard]] bool hasViewMenuItemEntry() const override { return false; }

        [[nodiscard]] ImVec2 getMinSize() const override { return scaled({ 600, 400 }); }
        [[nodiscard]] ImVec2 getMaxSize() const override { return scaled({ 900, 700 }); }

    private:
        void drawStore();
        void drawTab(StoreCategory &category);
        void handleDownloadFinished(const StoreCategory &category, StoreEntry &entry);

        void refresh();
        void parseResponse();

        void addCategory(const UnlocalizedString &unlocalizedName, const std::string &requestName, const paths::impl::DefaultPath *path, std::function<void()> downloadCallback = []{});

        bool download(const paths::impl::DefaultPath *pathType, const std::string &fileName, const std::string &url);
        bool remove(const paths::impl::DefaultPath *pathType, const std::string &fileName);
        void updateAll();

    private:
        HttpRequest m_httpRequest = HttpRequest("GET", "");
        std::future<HttpRequest::Result<std::string>> m_apiRequest;
        std::future<HttpRequest::Result<std::string>> m_download;
        std::fs::path m_downloadPath;
        RequestStatus m_requestStatus = RequestStatus::NotAttempted;

        std::vector<StoreCategory> m_categories;
        TaskHolder m_updateAllTask;
        std::atomic<u32> m_updateCount = 0;
    };

}