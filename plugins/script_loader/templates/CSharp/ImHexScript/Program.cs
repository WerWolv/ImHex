using ImHex;
using ImGuiNET;

class Script : IScript {
    
    public static int OnLoad()
    {
        // This function is executed the first time the Plugin is loaded

        return 1;
    }

    public static int Main()
    {
        // This function is executed when the plugin is selected in the "Run Script..." menu

        return 1;
    }
    
}