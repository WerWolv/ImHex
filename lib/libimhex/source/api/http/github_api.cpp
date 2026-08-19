#include <hex/api/http/github_api.hpp>

#include <hex/api/http/api_urls.hpp>
#include <hex/helpers/http_requests.hpp>

#include <nlohmann/json.hpp>

#include <map>
#include <memory>
#include <mutex>

namespace hex {

    namespace {

        using namespace std::literals::string_literals;
        using namespace std::literals::chrono_literals;

        struct GitHubState {
            std::mutex mutex;
            std::map<std::string, GitHubApi::ReleaseRequest> releaseRequests;
            std::map<std::string, GitHubApi::GistRequest> gistRequests;
            GitHubApi::ReleasesRequest releasesRequest;
            GitHubApi::CommitsRequest commitsRequest;
        };

        GitHubState& getGitHubState() {
            static GitHubState state;
            return state;
        }

        GitHubApi::Release parseRelease(const nlohmann::json &json) {
            GitHubApi::Release release {
                .name = json["name"].get<std::string>(),
                .tagName = json["tag_name"].get<std::string>(),
                .targetCommitish = json["target_commitish"].get<std::string>(),
                .body = json["body"].get<std::string>(),
                .assets = { }
            };

            for (const auto &asset : json["assets"]) {
                release.assets.push_back({
                    .name = asset["name"].get<std::string>(),
                    .browserDownloadUrl = asset["browser_download_url"].get<std::string>(),
                    .updatedAt = asset["updated_at"].get<std::string>()
                });
            }

            return release;
        }

        template<typename T, typename Parser>
        std::shared_future<GitHubApi::Result<T>> startRequest(std::string url, Parser parser) {
            auto httpRequest = std::make_shared<HttpRequest>("GET", std::move(url));
            httpRequest->setTimeout(30'000);
            auto request = httpRequest->execute();

            return std::async(std::launch::async, [httpRequest = std::move(httpRequest), request = std::move(request), parser = std::move(parser)]() mutable {
                static_cast<void>(httpRequest);
                auto response = request.get();
                if (!response.isSuccess()) {
                    const auto status = response.getStatusCode();
                    const auto httpStatus = std::get_if<HttpRequest::HttpStatus>(&status);
                    return GitHubApi::Result<T>(GitHubApi::Status::NetworkError, { }, status.toString(), httpStatus == nullptr ? 0 : u32(*httpStatus));
                }

                try {
                    return GitHubApi::Result<T>(GitHubApi::Status::Success, parser(nlohmann::json::parse(response.getData())));
                } catch (const nlohmann::json::exception &error) {
                    return GitHubApi::Result<T>(GitHubApi::Status::InvalidResponse, { }, error.what());
                }
            }).share();
        }

        GitHubApi::ReleaseRequest startReleaseRequest(const std::string &tag) {
            const auto path = tag.empty() ? "/releases/latest" : "/releases/tags/" + tag;
            return startRequest<GitHubApi::Release>(GitHubApiURL + path, parseRelease);
        }

        GitHubApi::ReleasesRequest startReleasesRequest() {
            return startRequest<std::vector<GitHubApi::Release>>(GitHubApiURL + "/releases"s, [](const auto &json) {
                std::vector<GitHubApi::Release> releases;
                for (const auto &release : json)
                    releases.push_back(parseRelease(release));
                return releases;
            });
        }

        GitHubApi::CommitsRequest startCommitsRequest() {
            return startRequest<std::vector<GitHubApi::Commit>>(GitHubApiURL + "/commits?per_page=100"s, [](const auto &json) {
                std::vector<GitHubApi::Commit> commits;
                for (const auto &entry : json) {
                    const auto fullMessage = entry["commit"]["message"].template get<std::string>();
                    const auto messageEnd = fullMessage.find("\n\n");
                    const auto message = messageEnd == std::string::npos ? fullMessage : fullMessage.substr(0, messageEnd);

                    commits.push_back({
                        .hash = entry["sha"].template get<std::string>(),
                        .message = message,
                        .description = messageEnd == std::string::npos ? "" : fullMessage.substr(messageEnd + 2),
                        .author = entry["commit"]["author"]["name"].template get<std::string>() + " <" + entry["commit"]["author"]["email"].template get<std::string>() + ">",
                        .date = entry["commit"]["author"]["date"].template get<std::string>(),
                        .url = entry["html_url"].template get<std::string>()
                    });
                }
                return commits;
            });
        }

        GitHubApi::GistRequest startGistRequest(const std::string &id) {
            return startRequest<GitHubApi::Gist>("https://api.github.com/gists/" + id, [](const auto &json) {
                GitHubApi::Gist gist;
                for (const auto &[name, file] : json["files"].items()) {
                    gist.files.push_back({ name, file["content"].template get<std::string>() });
                }
                return gist;
            });
        }

        GitHubApi::ReleaseRequest getReleaseRequest(const std::string &tag, bool refresh) {
            auto &state = getGitHubState();
            std::scoped_lock lock(state.mutex);
            auto &request = state.releaseRequests[tag];
            if (!request.valid() || (refresh && request.wait_for(0s) == std::future_status::ready))
                request = startReleaseRequest(tag);
            return request;
        }

        GitHubApi::GistRequest getGistRequest(const std::string &id, bool refresh) {
            auto &state = getGitHubState();
            std::scoped_lock lock(state.mutex);
            auto &request = state.gistRequests[id];
            if (!request.valid() || (refresh && request.wait_for(0s) == std::future_status::ready))
                request = startGistRequest(id);
            return request;
        }

    }

    GitHubApi::ReleaseRequest GitHubApi::getLatestRelease() {
        return getReleaseRequest({ }, false);
    }

    GitHubApi::ReleaseRequest GitHubApi::getRelease(const std::string &tag) {
        return getReleaseRequest(tag, false);
    }

    GitHubApi::ReleasesRequest GitHubApi::getReleases() {
        auto &state = getGitHubState();
        std::scoped_lock lock(state.mutex);
        if (!state.releasesRequest.valid())
            state.releasesRequest = startReleasesRequest();
        return state.releasesRequest;
    }

    GitHubApi::CommitsRequest GitHubApi::getCommits() {
        auto &state = getGitHubState();
        std::scoped_lock lock(state.mutex);
        if (!state.commitsRequest.valid())
            state.commitsRequest = startCommitsRequest();
        return state.commitsRequest;
    }

    GitHubApi::GistRequest GitHubApi::getGist(const std::string &id) {
        return getGistRequest(id, false);
    }

    GitHubApi::ReleaseRequest GitHubApi::refreshLatestRelease() {
        return getReleaseRequest({ }, true);
    }

    GitHubApi::ReleaseRequest GitHubApi::refreshRelease(const std::string &tag) {
        return getReleaseRequest(tag, true);
    }

    GitHubApi::ReleasesRequest GitHubApi::refreshReleases() {
        auto &state = getGitHubState();
        std::scoped_lock lock(state.mutex);
        if (!state.releasesRequest.valid() || state.releasesRequest.wait_for(0s) == std::future_status::ready)
            state.releasesRequest = startReleasesRequest();
        return state.releasesRequest;
    }

    GitHubApi::CommitsRequest GitHubApi::refreshCommits() {
        auto &state = getGitHubState();
        std::scoped_lock lock(state.mutex);
        if (!state.commitsRequest.valid() || state.commitsRequest.wait_for(0s) == std::future_status::ready)
            state.commitsRequest = startCommitsRequest();
        return state.commitsRequest;
    }

    GitHubApi::GistRequest GitHubApi::refreshGist(const std::string &id) {
        return getGistRequest(id, true);
    }

}
