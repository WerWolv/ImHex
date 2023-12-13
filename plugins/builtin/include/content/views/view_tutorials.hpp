#pragma once

#include <hex/ui/view.hpp>

#include <hex/api/tutorial_manager.hpp>

namespace hex::plugin::builtin {

    class ViewTutorials : public View::Floating {
    public:
        ViewTutorials();
        ~ViewTutorials() override = default;

        void drawContent() override;

        [[nodiscard]] bool shouldDraw() const override { return true; }
        [[nodiscard]] bool hasViewMenuItemEntry() const override { return false; }

        ImVec2 getMinSize() const override {
            return scaled({ 600, 400 });
        }

        ImVec2 getMaxSize() const override {
            return this->getMinSize();
        }

        ImGuiWindowFlags getWindowFlags() const override {
            return Floating::getWindowFlags() | ImGuiWindowFlags_NoResize;
        }

    private:
        const TutorialManager::Tutorial *m_selectedTutorial = nullptr;
    };

}