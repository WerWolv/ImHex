#pragma once

#include <hex.hpp>

#include <hex/ui/view.hpp>
#include <hex/helpers/net.hpp>
#include <hex/helpers/paths.hpp>

#include <array>
#include <future>
#include <string>
#include <filesystem>

namespace hex::plugin::builtin {

    struct StoreEntry {
        std::string name;
        std::string description;
        std::string fileName;
        std::string link;
        std::string hash;

        bool isFolder;

        bool downloading;
        bool installed;
        bool hasUpdate;
    };

    class ViewStore : public View {
    public:
        ViewStore();
        ~ViewStore() override;

        void drawContent() override;

        [[nodiscard]] bool isAvailable() const override { return true; }
        [[nodiscard]] bool hasViewMenuItemEntry() const override { return false; }

        [[nodiscard]] ImVec2 getMinSize() const override { return { 500, 400 }; }
        [[nodiscard]] ImVec2 getMaxSize() const override { return { 500, 400 }; }

    private:
        Net m_net;
        std::future<Response<std::string>> m_apiRequest;
        std::future<Response<void>> m_download;
        fs::path m_downloadPath;

        std::vector<StoreEntry> m_patterns, m_includes, m_magics, m_constants, m_yara, m_encodings;

        void drawStore();

        void refresh();
        void parseResponse();

        bool download(ImHexPath pathType, const std::string &fileName, const std::string &url, bool update);
        bool remove(ImHexPath pathType, const std::string &fileName);
    };

}