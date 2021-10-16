#include <hex/api/imhex_api.hpp>

#include <hex/api/event.hpp>
#include <hex/helpers/shared_data.hpp>

#include <unistd.h>

#include <hex/helpers/logger.hpp>

namespace hex {

    void ImHexApi::Common::sayHello() {
        log::warn("Hello!");
    }

    void ImHexApi::Common::closeImHex(bool noQuestions) {
        EventManager::post<RequestCloseImHex>(noQuestions);
    }

    void ImHexApi::Common::restartImHex() {
        EventManager::post<RequestCloseImHex>(false);
        std::atexit([]{
            execve(SharedData::mainArgv[0], SharedData::mainArgv, nullptr);
        });
    }


    void ImHexApi::Bookmarks::add(Region region, const std::string &name, const std::string &comment, u32 color) {
        Entry entry;

        entry.region = region;

        entry.name.reserve(name.length());
        entry.comment.reserve(comment.length());
        std::copy(name.begin(), name.end(), std::back_inserter(entry.name));
        std::copy(comment.begin(), comment.end(), std::back_inserter(entry.comment));
        entry.locked = false;

        entry.color = color;

        EventManager::post<RequestAddBookmark>(entry);
    }

    void ImHexApi::Bookmarks::add(u64 addr, size_t size, const std::string &name, const std::string &comment, u32 color) {
        Bookmarks::add(Region{addr, size}, name, comment, color);
    }

    std::list<ImHexApi::Bookmarks::Entry>& ImHexApi::Bookmarks::getEntries() {
        return SharedData::bookmarkEntries;
    }


    prv::Provider* ImHexApi::Provider::get() {
        if (!ImHexApi::Provider::isValid())
            return nullptr;

        return SharedData::providers[SharedData::currentProvider];
    }

    const std::vector<prv::Provider*>& ImHexApi::Provider::getProviders() {
        return SharedData::providers;
    }

    bool ImHexApi::Provider::isValid() {
        return !SharedData::providers.empty();
    }

    void ImHexApi::Provider::add(prv::Provider *provider) {
        SharedData::providers.push_back(provider);
        SharedData::currentProvider = SharedData::providers.size() - 1;
    }

    void ImHexApi::Provider::remove(prv::Provider *provider) {
        auto &providers = SharedData::providers;

        auto it = std::find(providers.begin(), providers.end(), provider);

        providers.erase(it);

        if (it - providers.begin() == SharedData::currentProvider)
            SharedData::currentProvider = 0;

        delete provider;
    }

}