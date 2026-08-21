#include <hex/api/http/github_api.hpp>

#include <hex/api/http/api_urls.hpp>
#include <hex/helpers/http_requests.hpp>

#include <nlohmann/json.hpp>

#include <condition_variable>
#include <map>
#include <mutex>
#include <optional>

namespace hex {

    namespace {

        using namespace std::literals::string_literals;
        using namespace std::literals::chrono_literals;

        template<typename T>
        struct RequestCache {
            std::condition_variable requestFinished;
            std::optional<GitHubApi::Result<T>> cachedResult;
            bool requestInProgress = false;
        };

        struct GitHubState {
            std::mutex mutex;
            std::map<std::string, RequestCache<GitHubApi::Release>> releaseRequests;
            std::map<std::string, RequestCache<GitHubApi::Gist>> gistRequests;
            RequestCache<std::vector<GitHubApi::Release>> releasesRequest;
            RequestCache<std::vector<GitHubApi::Commit>> commitsRequest;
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
        GitHubApi::Result<T> executeRequest(const std::string &url, Parser parser) {
            try {
                HttpRequest request("GET", url);
                request.setTimeout(30'000);
                const auto response = request.execute().get();
                if (!response.isSuccess()) {
                    const auto status = response.getStatusCode();
                    const auto httpStatus = std::get_if<HttpRequest::HttpStatus>(&status);
                    return GitHubApi::Result<T>(GitHubApi::Status::NetworkError, { }, status.toString(), httpStatus == nullptr ? 0 : u32(*httpStatus));
                }

                return GitHubApi::Result<T>(GitHubApi::Status::Success, parser(nlohmann::json::parse(response.getData())));
            } catch (const nlohmann::json::exception &error) {
                return GitHubApi::Result<T>(GitHubApi::Status::InvalidResponse, { }, error.what());
            } catch (const std::exception &error) {
                return GitHubApi::Result<T>(GitHubApi::Status::NetworkError, { }, error.what());
            }
        }

        GitHubApi::Result<GitHubApi::Release> loadRelease(const std::string &tag) {
            const auto path = tag.empty() ? "/releases/latest" : "/releases/tags/" + tag;
            return executeRequest<GitHubApi::Release>(GitHubApiURL + path, parseRelease);
        }

        GitHubApi::Result<std::vector<GitHubApi::Release>> loadReleases() {
            return executeRequest<std::vector<GitHubApi::Release>>(GitHubApiURL + "/releases"s, [](const auto &json) {
                std::vector<GitHubApi::Release> releases;
                for (const auto &release : json)
                    releases.push_back(parseRelease(release));
                return releases;
            });
        }

        GitHubApi::Result<std::vector<GitHubApi::Commit>> loadCommits() {
            return executeRequest<std::vector<GitHubApi::Commit>>(GitHubApiURL + "/commits?per_page=100"s, [](const auto &json) {
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

        GitHubApi::Result<GitHubApi::Gist> loadGist(const std::string &id) {
            return executeRequest<GitHubApi::Gist>("https://api.github.com/gists/" + id, [](const auto &json) {
                GitHubApi::Gist gist;
                for (const auto &[name, file] : json["files"].items()) {
                    gist.files.push_back({ name, file["content"].template get<std::string>() });
                }
                return gist;
            });
        }

        template<typename T>
        std::shared_future<GitHubApi::Result<T>> makeReadyRequest(const GitHubApi::Result<T> &result) {
            std::promise<GitHubApi::Result<T>> promise;
            promise.set_value(result);
            return promise.get_future().share();
        }

        template<typename T>
        std::shared_future<GitHubApi::Result<T>> waitForRequest(GitHubState &state, RequestCache<T> &cache) {
            return std::async(std::launch::async, [&state, &cache] {
                std::unique_lock lock(state.mutex);
                cache.requestFinished.wait(lock, [&cache] { return !cache.requestInProgress; });
                return *cache.cachedResult;
            }).share();
        }

        template<typename T, typename Loader>
        std::shared_future<GitHubApi::Result<T>> getCachedRequest(RequestCache<T> &cache, bool refresh, Loader loader) {
            auto &state = getGitHubState();
            std::scoped_lock lock(state.mutex);
            if (cache.requestInProgress)
                return waitForRequest(state, cache);
            if (!refresh && cache.cachedResult.has_value())
                return makeReadyRequest(*cache.cachedResult);

            cache.requestInProgress = true;
            return std::async(std::launch::async, [&state, &cache, loader = std::move(loader)]() mutable {
                auto result = loader();
                {
                    std::scoped_lock lock(state.mutex);
                    cache.cachedResult = result;
                    cache.requestInProgress = false;
                }
                cache.requestFinished.notify_all();
                return result;
            }).share();
        }

        GitHubApi::ReleaseRequest getReleaseRequest(const std::string &tag, bool refresh) {
            auto &state = getGitHubState();
            RequestCache<GitHubApi::Release> *cache;
            {
                std::scoped_lock lock(state.mutex);
                cache = &state.releaseRequests[tag];
            }
            return getCachedRequest(*cache, refresh, [tag] { return loadRelease(tag); });
        }

        GitHubApi::GistRequest getGistRequest(const std::string &id, bool refresh) {
            auto &state = getGitHubState();
            RequestCache<GitHubApi::Gist> *cache;
            {
                std::scoped_lock lock(state.mutex);
                cache = &state.gistRequests[id];
            }
            return getCachedRequest(*cache, refresh, [id] { return loadGist(id); });
        }

    }

    GitHubApi::ReleaseRequest GitHubApi::getLatestRelease() {
        return getReleaseRequest({ }, false);
    }

    GitHubApi::ReleaseRequest GitHubApi::getRelease(const std::string &tag) {
        return getReleaseRequest(tag, false);
    }

    GitHubApi::ReleasesRequest GitHubApi::getReleases() {
        return getCachedRequest(getGitHubState().releasesRequest, false, loadReleases);
    }

    GitHubApi::CommitsRequest GitHubApi::getCommits() {
        return getCachedRequest(getGitHubState().commitsRequest, false, loadCommits);
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
        return getCachedRequest(getGitHubState().releasesRequest, true, loadReleases);
    }

    GitHubApi::CommitsRequest GitHubApi::refreshCommits() {
        return getCachedRequest(getGitHubState().commitsRequest, true, loadCommits);
    }

    GitHubApi::GistRequest GitHubApi::refreshGist(const std::string &id) {
        return getGistRequest(id, true);
    }

}
