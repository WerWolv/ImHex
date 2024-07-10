#include <hex/helpers/utils.hpp>

#include <content/visualizer_helpers.hpp>

#include <imgui.h>
#include <hex/api/task_manager.hpp>
#include <hex/api/localization_manager.hpp>
#include <hex/helpers/http_requests.hpp>
#include <nlohmann/json.hpp>

#include <hex/ui/imgui_imhex_extensions.h>
#include <romfs/romfs.hpp>

namespace hex::plugin::visualizers {

    void drawCoordinateVisualizer(pl::ptrn::Pattern &, bool shouldReset, std::span<const pl::core::Token::Literal> arguments) {
        static ImVec2 coordinate;
        static double latitude, longitude;
        static std::string address;
        static std::mutex addressMutex;
        static TaskHolder addressTask;

        static auto mapTexture = ImGuiExt::Texture::fromImage(romfs::get("assets/common/map.jpg").span(), ImGuiExt::Texture::Filter::Linear);
        static ImVec2 mapSize = scaled(ImVec2(500, 500 / mapTexture.getAspectRatio()));

        if (shouldReset) {
            std::scoped_lock lock(addressMutex);

            address.clear();
            latitude  = arguments[0].toFloatingPoint();
            longitude = arguments[1].toFloatingPoint();

            // Convert latitude and longitude to X/Y coordinates on the image
            coordinate.x = float((longitude + 180) / 360 * mapSize.x);
            coordinate.y = float((-latitude + 90)  / 180 * mapSize.y);
        }

        const auto startPos = ImGui::GetWindowPos() + ImGui::GetCursorPos();

        // Draw background image
        ImGui::Image(mapTexture, mapSize);

        // Draw Longitude / Latitude text below image
        ImGui::PushTextWrapPos(startPos.x + mapSize.x);
        ImGuiExt::TextFormattedWrapped("{}: {:.0f}° {:.0f}' {:.4f}\" {}  |  {}: {:.0f}° {:.0f}' {:.4f}\" {}",
                             "hex.visualizers.pl_visualizer.coordinates.latitude"_lang,
                             std::floor(std::abs(latitude)),
                             std::floor(std::abs(latitude - std::floor(latitude)) * 60),
                             (std::abs(latitude - std::floor(latitude)) * 60 - std::floor(std::abs(latitude - std::floor(latitude)) * 60)) * 60,
                             latitude >= 0 ? "N" : "S",
                             "hex.visualizers.pl_visualizer.coordinates.longitude"_lang,
                             std::floor(std::abs(longitude)),
                             std::floor(std::abs(longitude - std::floor(longitude)) * 60),
                             (std::abs(longitude - std::floor(longitude)) * 60 - std::floor(std::abs(longitude - std::floor(longitude)) * 60)) * 60,
                             longitude >= 0 ? "E" : "W"
                             );
        ImGui::PopTextWrapPos();

        if (addressTask.isRunning()) {
            ImGuiExt::TextSpinner("hex.visualizers.pl_visualizer.coordinates.querying"_lang);
        } else if (address.empty()) {
            if (ImGuiExt::DimmedButton("hex.visualizers.pl_visualizer.coordinates.query"_lang)) {
                addressTask = TaskManager::createBackgroundTask("hex.visualizers.pl_visualizer.coordinates.querying"_lang, [lat = latitude, lon = longitude](auto &) {
                    constexpr static auto ApiURL = "https://geocode.maps.co/reverse?lat={}&lon={}&format=jsonv2";

                    HttpRequest request("GET", hex::format(ApiURL, lat, lon));
                    auto response = request.execute().get();

                    if (!response.isSuccess())
                        return;

                    try {

                        auto json = nlohmann::json::parse(response.getData());
                        auto jsonAddr = json["address"];

                        std::scoped_lock lock(addressMutex);
                        if (jsonAddr.contains("village")) {
                            address = hex::format("{} {}, {} {}",
                                                  jsonAddr["village"].get<std::string>(),
                                                  jsonAddr["county"].get<std::string>(),
                                                  jsonAddr["state"].get<std::string>(),
                                                  jsonAddr["country"].get<std::string>());
                        } else if (jsonAddr.contains("city")) {
                            address = hex::format("{}, {} {}, {} {}",
                                                  jsonAddr["road"].get<std::string>(),
                                                  jsonAddr["quarter"].get<std::string>(),
                                                  jsonAddr["city"].get<std::string>(),
                                                  jsonAddr["state"].get<std::string>(),
                                                  jsonAddr["country"].get<std::string>());
                        }
                    } catch (std::exception &) {
                        address = std::string("hex.visualizers.pl_visualizer.coordinates.querying_no_address"_lang);
                    }
                });
            }
        } else {
            ImGui::PushTextWrapPos(startPos.x + mapSize.x);
            ImGuiExt::TextFormattedWrapped("{}", address);
            ImGui::PopTextWrapPos();
        }

        // Draw crosshair pointing to the coordinates
        {
            constexpr static u32 CrossHairColor = 0xFF00D0D0;
            constexpr static u32 BorderColor    = 0xFF000000;

            auto drawList = ImGui::GetWindowDrawList();
            drawList->AddLine(startPos + ImVec2(coordinate.x, 0), startPos + ImVec2(coordinate.x, mapSize.y), CrossHairColor, 2_scaled);
            drawList->AddLine(startPos + ImVec2(0, coordinate.y), startPos + ImVec2(mapSize.x, coordinate.y), CrossHairColor, 2_scaled);
            drawList->AddCircleFilled(startPos + coordinate, 5, CrossHairColor);
            drawList->AddCircle(startPos + coordinate, 5, BorderColor);
        }
    }

}
