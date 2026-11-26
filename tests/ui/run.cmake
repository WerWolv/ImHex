# Use a separate profile so tests do not depend on a user's ImHex settings.
file(MAKE_DIRECTORY "${IMHEX_UI_TEST_PROFILE}/config/imhex/config"
                    "${IMHEX_UI_TEST_PROFILE}/data"
                    "${IMHEX_UI_TEST_PROFILE}/cache")
file(WRITE "${IMHEX_UI_TEST_PROFILE}/config/imhex/config/settings.json"
    "{\n"
    "  \"hex.builtin.setting.general\": {\n"
    "    \"hex.builtin.setting.general.prev_launch_version\": \"${IMHEX_UI_TEST_VERSION}\",\n"
    "    \"hex.builtin.setting.general.server_contact\": 2,\n"
    "    \"hex.builtin.setting.general.show_tips\": false\n"
    "  },\n"
    "  \"hex.builtin.setting.interface\": {\n"
    "    \"hex.builtin.setting.interface.language\": \"en-US\",\n"
    "    \"hex.builtin.setting.interface.multi_windows\": false\n"
    "  }\n"
    "}\n")

set(command "${IMHEX_EXECUTABLE}" --run-ui-tests)
if (IMHEX_XVFB_RUN)
    set(command "${IMHEX_XVFB_RUN}" -a -s "-screen 0 1600x1000x24" ${command})
endif ()

# The debug warning banner can cover controls that UI tests need to click.
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env
        "XDG_CONFIG_HOME=${IMHEX_UI_TEST_PROFILE}/config"
        "XDG_DATA_HOME=${IMHEX_UI_TEST_PROFILE}/data"
        "XDG_CACHE_HOME=${IMHEX_UI_TEST_PROFILE}/cache"
        "IMHEX_SKIP_SPLASH_SCREEN=1"
        "NO_DEBUG_BANNER=1"
        ${command}
    RESULT_VARIABLE result)
if (NOT result STREQUAL "0")
    message(FATAL_ERROR "UI test runner failed: ${result}")
endif ()
