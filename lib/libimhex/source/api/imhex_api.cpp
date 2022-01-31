#include <hex/api/imhex_api.hpp>

#include <hex/api/event.hpp>
#include <hex/helpers/shared_data.hpp>

#include <unistd.h>

#include <hex/helpers/logger.hpp>

namespace hex {

    namespace ImHexApi::Common {

        void closeImHex(bool noQuestions) {
            EventManager::post<RequestCloseImHex>(noQuestions);
        }

        void restartImHex() {
            EventManager::post<RequestCloseImHex>(false);
            std::atexit([] {
                auto &programArgs = ImHexApi::System::getProgramArguments();
                execve(programArgs.argv[0], programArgs.argv, programArgs.envp);
            });
        }

    }


    namespace ImHexApi::HexEditor {

        static std::map<u32, ImHexApi::HexEditor::Highlighting> s_highlights;

        Highlighting::Highlighting(Region region, color_t color, const std::string &tooltip)
                : m_region(region), m_color(color), m_tooltip(tooltip) {
        }

        u32 addHighlight(const Region &region, color_t color, std::string tooltip) {
            auto id = s_highlights.size();

            s_highlights.insert({ id, Highlighting{ region, color, tooltip } });

            return id;
        }

        void removeHighlight(u32 id) {
            s_highlights.erase(id);
        }

        std::map<u32, Highlighting> &getHighlights() {
            return s_highlights;
        }

    }


    namespace ImHexApi::Bookmarks {

        void add(Region region, const std::string &name, const std::string &comment, u32 color) {
            EventManager::post<RequestAddBookmark>(region, name, comment, color);
        }

        void add(u64 address, size_t size, const std::string &name, const std::string &comment, u32 color) {
            add(Region { address, size }, name, comment, color);
        }

    }


    namespace ImHexApi::Provider {

        static u32 s_currentProvider;
        static std::vector<prv::Provider*> s_providers;

        prv::Provider *get() {
            if (!ImHexApi::Provider::isValid())
                return nullptr;

            return s_providers[s_currentProvider];
        }

        const std::vector<prv::Provider *> &getProviders() {
            return s_providers;
        }

        void setCurrentProvider(u32 index) {
            if (index < s_providers.size()) {
                auto oldProvider = get();
                s_currentProvider = index;
                EventManager::post<EventProviderChanged>(oldProvider, get());
            }
        }

        bool isValid() {
            return !s_providers.empty();
        }

        void add(prv::Provider *provider) {
            s_providers.push_back(provider);
            setCurrentProvider(s_providers.size() - 1);

            EventManager::post<EventProviderCreated>(provider);
        }

        void remove(prv::Provider *provider) {
            auto it = std::find(s_providers.begin(), s_providers.end(), provider);

            s_providers.erase(it);

            if (it - s_providers.begin() == s_currentProvider)
                s_currentProvider = 0;

            delete provider;
        }

    }


    namespace ImHexApi::Tasks {

        Task createTask(const std::string &unlocalizedName, u64 maxValue) {
            return Task(unlocalizedName, maxValue);
        }


        std::vector<std::function<void()>> s_deferredCalls;

        void doLater(const std::function<void()> &function) {
            getDeferredCalls().push_back(function);
        }

        std::vector<std::function<void()>>& getDeferredCalls() {
            return s_deferredCalls;
        }

    }


    namespace ImHexApi::System {

        static ProgramArguments s_programArguments;

        void setProgramArguments(int argc, char **argv, char **envp) {
            s_programArguments.argc = argc;
            s_programArguments.argv = argv;
            s_programArguments.envp = envp;
        }

        const ProgramArguments& getProgramArguments() {
            return s_programArguments;
        }


        static float s_targetFPS;

        float getTargetFPS() {
            return s_targetFPS;
        }

        void setTargetFPS(float fps) {
            s_targetFPS = fps;
        }

    }

}
