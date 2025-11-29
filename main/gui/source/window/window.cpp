#include "window.hpp"

#include <hex.hpp>

#include <hex/api/plugin_manager.hpp>
#include <hex/api/content_registry/views.hpp>
#include <hex/api/imhex_api/fonts.hpp>
#include <hex/api/layout_manager.hpp>
#include <hex/api/shortcut_manager.hpp>
#include <hex/api/workspace_manager.hpp>
#include <hex/api/tutorial_manager.hpp>
#include <hex/api/events/requests_lifecycle.hpp>
#include <hex/api/events/events_lifecycle.hpp>
#include <hex/api/events/requests_gui.hpp>
#include <hex/api/events/events_gui.hpp>

#include <hex/helpers/utils.hpp>
#include <hex/helpers/logger.hpp>
#include <hex/helpers/default_paths.hpp>

#include <hex/providers/provider.hpp>

#include <hex/ui/view.hpp>
#include <hex/ui/popup.hpp>
#include <hex/ui/banner.hpp>

#if defined(OS_WEB)
    #include <hex/helpers/web_clipboard.hpp>
#endif

#include <cmath>
#include <chrono>
#include <csignal>
#include <numbers>

#include <romfs/romfs.hpp>

#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <hex/ui/imgui_imhex_extensions.h>
#include <implot.h>
#include <implot_internal.h>
#include <implot3d.h>
#include <implot3d_internal.h>
#include <imnodes.h>
#include <imnodes_internal.h>

#if defined(IMGUI_TEST_ENGINE)
    #include <imgui_te_engine.h>
    #include <imgui_te_ui.h>
#endif