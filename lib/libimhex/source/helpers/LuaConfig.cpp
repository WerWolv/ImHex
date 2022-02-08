#include <hex/helpers/LuaConfig.hpp>


static std::shared_ptr<LuaConfig> singleton = nullptr;
static std::mutex singletonMutex;

std::shared_ptr<LuaConfig> LuaConfig::getLuaConfig() {
    if (singleton == nullptr) {
        std::unique_lock<std::mutex> lock(singletonMutex);
        if (singleton == nullptr) {
            auto temp = std::shared_ptr<LuaConfig>(new LuaConfig());
            singleton = temp;
        }
    }
    return singleton;
}
