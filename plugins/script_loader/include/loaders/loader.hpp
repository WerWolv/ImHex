#pragma once

#include <functional>
#include <string>
#include <vector>

namespace hex::script::loader {

    struct Script {
        std::string name;
        std::function<void()> entryPoint;
    };

    class ScriptLoader {
    public:
        ScriptLoader() = default;
        virtual ~ScriptLoader() = default;

        virtual bool initialize() = 0;
        virtual bool loadAll() = 0;

        void addScript(std::string name, std::function<void()> entryPoint) {
            this->m_scripts.emplace_back(std::move(name), std::move(entryPoint));
        }

        const auto& getScripts() const {
            return this->m_scripts;
        }

    protected:
        void clearScripts() {
            this->m_scripts.clear();
        }

    private:
        std::vector<Script> m_scripts;
    };

}