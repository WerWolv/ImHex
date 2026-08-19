#pragma once

#include <hex.hpp>

#include <future>
#include <string>
#include <utility>
#include <vector>

EXPORT_MODULE namespace hex {

    class GitHubApi {
    public:
        enum class Status {
            Success,
            NetworkError,
            InvalidResponse
        };

        struct Asset {
            std::string name;
            std::string browserDownloadUrl;
            std::string updatedAt;
        };

        struct Release {
            std::string name;
            std::string tagName;
            std::string targetCommitish;
            std::string body;
            std::vector<Asset> assets;
        };

        struct Commit {
            std::string hash;
            std::string message;
            std::string description;
            std::string author;
            std::string date;
            std::string url;
        };

        struct GistFile {
            std::string name;
            std::string content;
        };

        struct Gist {
            std::vector<GistFile> files;
        };

        template<typename T>
        class Result {
        public:
            explicit Result(Status status, T data = { }, std::string errorMessage = { }, u32 statusCode = 0) :
                m_status(status), m_data(std::move(data)), m_errorMessage(std::move(errorMessage)), m_statusCode(statusCode) { }

            [[nodiscard]] bool isSuccess() const { return m_status == Status::Success; }
            [[nodiscard]] Status getStatus() const { return m_status; }
            [[nodiscard]] const T& getData() const { return m_data; }
            [[nodiscard]] const std::string& getErrorMessage() const { return m_errorMessage; }
            [[nodiscard]] u32 getStatusCode() const { return m_statusCode; }

        private:
            Status m_status;
            T m_data;
            std::string m_errorMessage;
            u32 m_statusCode;
        };

        using ReleaseRequest = std::shared_future<Result<Release>>;
        using ReleasesRequest = std::shared_future<Result<std::vector<Release>>>;
        using CommitsRequest = std::shared_future<Result<std::vector<Commit>>>;
        using GistRequest = std::shared_future<Result<Gist>>;

        static ReleaseRequest getLatestRelease();
        static ReleaseRequest getRelease(const std::string &tag);
        static ReleasesRequest getReleases();
        static CommitsRequest getCommits();
        static GistRequest getGist(const std::string &id);

        static ReleaseRequest refreshLatestRelease();
        static ReleaseRequest refreshRelease(const std::string &tag);
        static ReleasesRequest refreshReleases();
        static CommitsRequest refreshCommits();
        static GistRequest refreshGist(const std::string &id);

    private:
        GitHubApi() = delete;
    };

}
