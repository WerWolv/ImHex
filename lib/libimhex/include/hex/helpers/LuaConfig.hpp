
#include <iostream>
#include <memory>
#include <mutex>

#include <selene.h>
#include <cassert>

#include <hex/helpers/paths.hpp>


// curl -R -O http://www│
// .lua.org/ftp/lua-5.3.0.tar.gz
// tar zxf lua-5.3.0.tar.gz
// cd lua-5.3.0
// make macosx test
// make install

class LuaConfig {

public:

    static std::shared_ptr<LuaConfig> getLuaConfig();

    // void print() {
    //     std::cout << "Hello World." << std::endl;
    // }

    ~LuaConfig() {
        // std::cout << __PRETTY_FUNCTION__ << std::endl;
    }
    template<class T>
    const T get_key_value(const char *dict, const char* key){
        return static_cast<T>(state[dict][key]);
    }
private:

    LuaConfig() {
        std::string path = hex::getExecutablePath() ;
        path += std::string("/config.lua");
        // printf(path.c_str());
        state.Load(path);
    }
    sel::State state;
};