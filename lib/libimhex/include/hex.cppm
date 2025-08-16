module;

#include <cmath>
#include <map>
#include <string>
#include <string_view>
#include <vector>
#include <exception>
#include <algorithm>
#include <locale>
#include <array>
#include <filesystem>
#include <functional>
#include <memory>
#include <list>
#include <atomic>
#include <ranges>
#include <fstream>
#include <thread>
#include <future>

#include <fmt/format.h>
#include <nlohmann/json.hpp>
#include <wolv/io/file.hpp>

#include <imgui.h>
#include <imgui_internal.h>
#include <hex/ui/imgui_imhex_extensions.h>

#include <hex/helpers/auto_reset.hpp>
#include <hex/api/event_manager.hpp>
#include <hex/providers/provider.hpp>
#include <hex/providers/provider_data.hpp>
#include <hex/data_processor/node.hpp>
#include <hex/data_processor/link.hpp>
#include <hex/data_processor/attribute.hpp>
#include <pl/pattern_language.hpp>

export module hex;

#define HEX_MODULE_EXPORT

#include <hex/api/imhex_api/bookmarks.hpp>
#include <hex/api/imhex_api/hex_editor.hpp>
#include <hex/api/imhex_api/fonts.hpp>
#include <hex/api/imhex_api/messaging.hpp>
#include <hex/api/imhex_api/provider.hpp>
#include <hex/api/imhex_api/system.hpp>

#include <hex/api/task_manager.hpp>
#include <hex/api/achievement_manager.hpp>
#include <hex/api/layout_manager.hpp>
#include <hex/api/localization_manager.hpp>
#include <hex/api/plugin_manager.hpp>
#include <hex/api/shortcut_manager.hpp>
#include <hex/api/theme_manager.hpp>
#include <hex/api/tutorial_manager.hpp>
#include <hex/api/workspace_manager.hpp>
