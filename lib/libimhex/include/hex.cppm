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

#include <fmt/format.h>
#include <nlohmann/json.hpp>

export module hex;

#define HEX_MODULE_EXPORT
#include <hex/api/achievement_manager.hpp>
#include <hex/api/content_registry.hpp>
#include <hex/api/event_manager.hpp>
#include <hex/api/imhex_api.hpp>
#include <hex/api/layout_manager.hpp>
#include <hex/api/localization_manager.hpp>
#include <hex/api/plugin_manager.hpp>
#include <hex/api/shortcut_manager.hpp>
#include <hex/api/task_manager.hpp>
#include <hex/api/theme_manager.hpp>
#include <hex/api/tutorial_manager.hpp>
#include <hex/api/workspace_manager.hpp>