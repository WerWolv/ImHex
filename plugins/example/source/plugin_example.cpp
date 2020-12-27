#include <plugin.hpp>

#include <views/view.hpp>

class ViewExample : public hex::View {
public:
    ViewExample() : hex::View("Example") {}
    ~ViewExample() override {}

    void drawContent() override {
        if (ImGui::Begin("Example")) {
            ImGui::Text("Custom plugin window");
        }
        ImGui::End();
    }
};

IMHEX_PLUGIN {

    View* createView() {
        return new ViewExample();
    }

    void drawToolsEntry() {
        if (ImGui::CollapsingHeader("Example Tool")) {
            ImGui::Text("Custom Plugin tool");
        }
    }

}


