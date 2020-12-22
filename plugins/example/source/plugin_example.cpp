#include <hex.hpp>

#include <views/view.hpp>
#include <imgui.h>

class ViewExample : public hex::View {
public:
    ViewExample() : hex::View("Example") {}
    ~ViewExample() override {}

    void createView() override {
        if (ImGui::Begin("Example")) {
            ImGui::Text("Example View");
        }
        ImGui::End();
    }
};

namespace hex::plugin {

    View* createView() {
        return new ViewExample();
    }

}


