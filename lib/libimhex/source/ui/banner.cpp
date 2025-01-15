#include <hex/ui/banner.hpp>
#include <hex/helpers/auto_reset.hpp>

namespace hex::impl {

    [[nodiscard]] std::list<std::unique_ptr<BannerBase>> &BannerBase::getOpenBanners() {
        static AutoReset<std::list<std::unique_ptr<BannerBase>>> openBanners;

        return openBanners;
    }

    std::mutex& BannerBase::getMutex() {
        static std::mutex mutex;

        return mutex;
    }

}