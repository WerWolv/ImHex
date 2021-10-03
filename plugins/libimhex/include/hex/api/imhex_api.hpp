#pragma once

#include <hex.hpp>

#include <list>
#include <vector>
#include <string>

#include <hex/helpers/concepts.hpp>

namespace hex {

    namespace prv { class Provider; }

    namespace ImHexApi {
        namespace Common {

            void sayHello();
            void closeImHex(bool noQuestions = false);
            void restartImHex();

        };

        namespace Bookmarks {
            struct Entry {
                Region region;

                std::vector<char> name;
                std::vector<char> comment;
                u32 color;
                bool locked;
            };

            void add(Region region, const std::string &name, const std::string &comment, u32 color = 0x00000000);
            void add(u64 addr, size_t size, const std::string &name, const std::string &comment, u32 color = 0x00000000);

            std::list<Entry>& getEntries();
        };

        namespace Provider {

            prv::Provider* get();
            const std::vector<prv::Provider*>& getProviders();

            bool isValid();

            void add(prv::Provider *provider);

            template<hex::derived_from<prv::Provider> T>
            void add(auto&& ... args) {
                add(new T(std::forward<decltype(args)>(args)...));
            }

            void remove(prv::Provider *provider);

        };

    };

}
