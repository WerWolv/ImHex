#pragma once

#include <hex.hpp>
#include <hex/api/localization_manager.hpp>

#include <memory>
#include <list>
#include <string>

namespace hex::proj {

    class Content {
    public:
        explicit Content(UnlocalizedString type, std::string name)
            : m_type(std::move(type)), m_name(std::move(name)) { }

        void setData(std::string data) { m_data = std::move(data); }
        const std::string& getData() const { return m_data; }

        const std::string& getName() const { return m_name; }
        void setName(std::string name) { m_name = std::move(name); }

        const UnlocalizedString& getType() const { return m_type; }

        bool isOpen() const { return m_open; }
        void setOpen(bool open) { m_open = open; }

        bool isEmpty() const { return m_data.empty(); }

    private:
        UnlocalizedString m_type;
        std::string m_name;
        std::string m_data;
        bool m_open = false;
    };

    class Project {
    public:
        explicit Project(std::string name) : m_name(std::move(name)) {}
        Project(const Project &) = delete;
        Project(Project &&) = delete;
        ~Project();

        Project &operator=(const Project &) = delete;
        Project &operator=(Project &&) = delete;

        const std::string &getName() const { return m_name; }

        void addContent(UnlocalizedString type);
        const std::list<std::unique_ptr<Content>>& getContents() const { return m_contents; }

    private:
        std::string m_name;
        std::list<std::unique_ptr<Content>> m_contents;
    };

}
