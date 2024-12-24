#pragma once

#include <hex/providers/undo_redo/operations/operation.hpp>

#include <hex/helpers/fmt.hpp>

namespace hex::plugin::builtin::undo {

    class OperationBookmark : public prv::undo::Operation {
    public:
        explicit OperationBookmark(ImHexApi::Bookmarks::Entry entry) :
            m_entry(std::move(entry)) { }

        void undo(prv::Provider *provider) override {
            std::ignore = provider;

            ImHexApi::Bookmarks::remove(m_entry.id);
        }

        void redo(prv::Provider *provider) override {
            std::ignore = provider;

            auto &[region, name, comment, color, locked, id] = m_entry;

            id = ImHexApi::Bookmarks::add(region, name, comment, color);
        }

        [[nodiscard]] std::string format() const override {
            return hex::format("Bookmark {} created", m_entry.name);
        }

        std::unique_ptr<Operation> clone() const override {
            return std::make_unique<OperationBookmark>(*this);
        }

        [[nodiscard]] Region getRegion() const override {
            return m_entry.region;
        }

        bool shouldHighlight() const override { return false; }

    private:
        ImHexApi::Bookmarks::Entry m_entry;
    };

}