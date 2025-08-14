#pragma once

#include <hex.hpp>

#include <hex/helpers/fs.hpp>

#include <vector>
#include <string>
#include <functional>

EXPORT_MODULE namespace hex {

    /* File Handler Registry. Allows adding handlers for opening files specific file types */
    namespace ContentRegistry::FileTypeHandler {

        namespace impl {

            using Callback = std::function<bool(std::fs::path)>;
            struct Entry {
                std::vector<std::string> extensions;
                Callback callback;
            };

            const std::vector<Entry>& getEntries();

        }

        /**
         * @brief Adds a new file handler
         * @param extensions The file extensions to handle
         * @param callback The function to call to handle the file
         */
        void add(const std::vector<std::string> &extensions, const impl::Callback &callback);

    }

}