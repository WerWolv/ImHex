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

IMHEX_PLUGIN_SETUP {

    ContentRegistry::Views::add<ViewExample>();

}


