#pragma once

#include <wolv/types/static_string.hpp>
#include <wolv/utils/preproc.hpp>

#include <hex/ui/imgui_imhex_extensions.h>


#if defined(DEBUG)
    #define DBG_DEFINE_DEBUG_VARIABLE(type, name)                       \
        static type name;                                               \
        hex::dbg::impl::drawDebugVariable(name, WOLV_STRINGIFY(name));
#else
    #define DBG_DEFINE_DEBUG_VARIABLE(type, name)                       \
        static_assert(false, "Debug variables are only intended for use during development.");
#endif

namespace hex::dbg {

    namespace impl {
        bool &getDebugWindowState();

        template<typename T>
        static void drawDebugVariable(T &variable, std::string_view name) {
            if (!getDebugWindowState())
                return;

            if (ImGui::Begin("Debug Variables", &getDebugWindowState(), ImGuiWindowFlags_AlwaysAutoResize)) {
                using Type = std::remove_cvref_t<T>;
                if constexpr (std::same_as<Type, bool>) {
                    ImGui::Checkbox(name.data(), &variable);
                } else if constexpr (std::integral<Type> || std::floating_point<Type>) {
                    ImGui::InputScalar(name.data(), ImGui::getImGuiDataType<Type>(), &variable);
                } else if constexpr (std::same_as<Type, ImVec2>) {
                    ImGui::InputFloat2(name.data(), &variable.x);
                } else if constexpr (std::same_as<Type, std::string>) {
                    ImGui::InputText(name.data(), variable);
                } else if constexpr (std::same_as<Type, ImColor>) {
                    ImGui::ColorEdit4(name.data(), &variable.Value.x, ImGuiColorEditFlags_AlphaBar);
                } else {
                    static_assert(hex::always_false<Type>::value, "Unsupported type");
                }
            }
            ImGui::End();
        }
    }

}