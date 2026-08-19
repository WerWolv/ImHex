#pragma once

#include <hex.hpp>
#include <hex/helpers/default_paths.hpp>

#include <filesystem>
#include <future>
#include <map>
#include <string>
#include <utility>
#include <vector>

EXPORT_MODULE namespace hex {

    class StoreApi {
    public:
        using Pragmas = std::multimap<std::string, std::string>;

        struct Entry {
            std::string name;
            std::string description;
            std::vector<std::string> authors;
            std::string fileName;
            std::string link;
            std::string hash;
            bool isFolder;
            Pragmas pragmas;
        };

        using Categories = std::map<std::string, std::vector<Entry>>;

        struct Store {
            Categories categories;
        };

        class Result {
        public:
            enum class Status {
                Success,
                NetworkError,
                InvalidResponse
            };

            explicit Result(Status status, Store data = { }, std::string errorMessage = { }) :
                m_status(status), m_data(std::move(data)), m_errorMessage(std::move(errorMessage)) { }

            [[nodiscard]] bool isSuccess() const { return m_status == Status::Success; }
            [[nodiscard]] Status getStatus() const { return m_status; }
            [[nodiscard]] const Store& getData() const { return m_data; }
            [[nodiscard]] const std::string& getErrorMessage() const { return m_errorMessage; }

        private:
            Status m_status;
            Store m_data;
            std::string m_errorMessage;
        };

        using Request = std::shared_future<Result>;

        class DownloadResult {
        public:
            enum class Status {
                Success,
                NoWritablePath,
                NetworkError
            };

            explicit DownloadResult(Status status, std::fs::path path = { }, std::string errorMessage = { }) :
                m_status(status), m_path(std::move(path)), m_errorMessage(std::move(errorMessage)) { }

            [[nodiscard]] bool isSuccess() const { return m_status == Status::Success; }
            [[nodiscard]] Status getStatus() const { return m_status; }
            [[nodiscard]] const std::fs::path& getPath() const { return m_path; }
            [[nodiscard]] const std::string& getErrorMessage() const { return m_errorMessage; }

        private:
            Status m_status;
            std::fs::path m_path;
            std::string m_errorMessage;
        };

        using DownloadRequest = std::future<DownloadResult>;

        /**
         * Gets the store contents. Repeated calls return the cached request and result.
         */
        static Request get();

        /**
         * Refreshes the store contents. If a request is already running, it is reused.
         */
        static Request refresh();

        static DownloadRequest download(const paths::impl::DefaultPath *pathType, const std::string &fileName, const std::string &url);

    private:
        StoreApi() = delete;
    };

}
