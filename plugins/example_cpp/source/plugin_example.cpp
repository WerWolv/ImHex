#include <hex/plugin.hpp>
#include <hex/api/content_registry.hpp>
#include <hex/ui/view.hpp>

class ViewExample : public hex::View {
public:
    ViewExample() : hex::View("Example") { }
    ~ViewExample() override { }

    void drawContent() override {
        if (ImGui::Begin("Example")) {
            ImGui::TextUnformatted("Custom plugin window");
        }
        ImGui::End();
    }
};

IMHEX_PLUGIN_SETUP("Example C++", "WerWolv", "Example C++ plugin used as template for plugin devs") {

    hex::ContentRegistry::Views::add<ViewExample>();
}
