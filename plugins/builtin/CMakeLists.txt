cmake_minimum_required(VERSION 3.16)

include(ImHexPlugin)

find_file(DEFAULT_MAGIC_FILE_PATH magic.mgc HINTS ${LIBMAGIC_INCLUDE_DIR}/../share/misc)
if (EXISTS ${DEFAULT_MAGIC_FILE_PATH})
    add_romfs_resource(${DEFAULT_MAGIC_FILE_PATH} always_auto_extract/magic/magic.mgc)
endif ()

add_imhex_plugin(
    NAME
        builtin

    SOURCES
        source/plugin_builtin.cpp

        source/content/background_services.cpp
        source/content/command_palette_commands.cpp
        source/content/command_line_interface.cpp
        source/content/communication_interface.cpp
        source/content/data_inspector.cpp
        source/content/pl_builtin_functions.cpp
        source/content/pl_builtin_types.cpp
        source/content/pl_pragmas.cpp
        source/content/settings_entries.cpp
        source/content/tools_entries.cpp
        source/content/data_processor_nodes.cpp
        source/content/ui_items.cpp
        source/content/providers.cpp
        source/content/views.cpp
        source/content/data_formatters.cpp
        source/content/main_menu_items.cpp
        source/content/welcome_screen.cpp
        source/content/data_visualizers.cpp
        source/content/events.cpp
        source/content/global_actions.cpp
        source/content/themes.cpp
        source/content/recent.cpp
        source/content/file_handlers.cpp
        source/content/project.cpp
        source/content/achievements.cpp
        source/content/file_extraction.cpp
        source/content/report_generators.cpp
        source/content/init_tasks.cpp
        source/content/workspaces.cpp
        source/content/pl_visualizers.cpp
        source/content/out_of_box_experience.cpp
        source/content/minimap_visualizers.cpp
        source/content/window_decoration.cpp
        source/content/data_information_sections.cpp

        source/content/helpers/demangle.cpp

        source/content/data_processor_nodes/basic_nodes.cpp
        source/content/data_processor_nodes/control_nodes.cpp
        source/content/data_processor_nodes/decode_nodes.cpp
        source/content/data_processor_nodes/logic_nodes.cpp
        source/content/data_processor_nodes/math_nodes.cpp
        source/content/data_processor_nodes/other_nodes.cpp
        source/content/data_processor_nodes/visual_nodes.cpp

        source/content/providers/file_provider.cpp
        source/content/providers/gdb_provider.cpp
        source/content/providers/disk_provider.cpp
        source/content/providers/intel_hex_provider.cpp
        source/content/providers/motorola_srec_provider.cpp
        source/content/providers/memory_file_provider.cpp
        source/content/providers/process_memory_provider.cpp
        source/content/providers/base64_provider.cpp
        source/content/providers/view_provider.cpp

        source/content/tools/ascii_table.cpp
        source/content/tools/base_converter.cpp
        source/content/tools/byte_swapper.cpp
        source/content/tools/color_picker.cpp
        source/content/tools/demangler.cpp
        source/content/tools/euclidean_alg.cpp
        source/content/tools/file_tool_shredder.cpp
        source/content/tools/file_tool_splitter.cpp
        source/content/tools/file_tool_combiner.cpp
        source/content/tools/file_uploader.cpp
        source/content/tools/graphing_calc.cpp
        source/content/tools/ieee_decoder.cpp
        source/content/tools/math_eval.cpp
        source/content/tools/multiplication_decoder.cpp
        source/content/tools/perms_calc.cpp
        source/content/tools/regex_replacer.cpp
        source/content/tools/tcp_client_server.cpp
        source/content/tools/wiki_explainer.cpp
        source/content/tools/http_requests.cpp

        source/content/pl_visualizers/hex_viewer.cpp
        source/content/pl_visualizers/chunk_entropy.cpp

        source/content/tutorials/tutorials.cpp
        source/content/tutorials/introduction.cpp

        source/content/popups/hex_editor/popup_hex_editor_find.cpp

        source/content/views/view_hex_editor.cpp
        source/content/views/view_pattern_editor.cpp
        source/content/views/view_pattern_data.cpp
        source/content/views/view_information.cpp
        source/content/views/view_about.cpp
        source/content/views/view_tools.cpp
        source/content/views/view_data_inspector.cpp
        source/content/views/view_bookmarks.cpp
        source/content/views/view_patches.cpp
        source/content/views/view_command_palette.cpp
        source/content/views/view_settings.cpp
        source/content/views/view_data_processor.cpp
        source/content/views/view_constants.cpp
        source/content/views/view_store.cpp
        source/content/views/view_provider_settings.cpp
        source/content/views/view_find.cpp
        source/content/views/view_theme_manager.cpp
        source/content/views/view_logs.cpp
        source/content/views/view_achievements.cpp
        source/content/views/view_highlight_rules.cpp
        source/content/views/view_tutorials.cpp
    INCLUDES
        include

    LIBRARIES
        ui
        fonts
        ${JTHREAD_LIBRARIES}
        plcli
        libpl-gen
        LLVMDemangle
)

if (WIN32)
    target_link_libraries(builtin PRIVATE setupapi)
endif ()