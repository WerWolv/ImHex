#include <hex/plugin.hpp>

// Browse through the headers in lib/libimhex/include/hex/api/ to see what you can do with the API.
// Most important ones are <hex/api/imhex_api.hpp> and <hex/api/content_registry.hpp>

// This is the main entry point of your plugin. The code in the body of this construct will be executed
// when ImHex starts up and loads the plugin.
// The strings in the header are used to display information about the plugin in the UI.
IMHEX_PLUGIN_SETUP("Example Plugin", "Author", "Description") {
    // Put your init code here
}