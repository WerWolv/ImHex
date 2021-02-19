#include <hex/plugin.hpp>

#include <hex/views/view.hpp>

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

IMHEX_PLUGIN_SETUP("Example", "WerWolv", "Example plugin used as template for plugin devs") {

    ContentRegistry::Views::add<ViewExample>();

}


