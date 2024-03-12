using ImHex;
using ImGuiNET;
class Script {
    
    public static void OnLoad() {
        // This function is executed the first time the Plugin is loaded
        UI.RegisterView(new byte[]{ 0xEE, 0xAC, 0x89 }, "Test View", () =>
        {
            ImGui.SetCurrentContext(UI.GetImGuiContext());
            ImGui.TextUnformatted("Test Text");
            if (ImGui.Button("Hello World"))
            {
                UI.ShowToast("Hello World");
            }
        });
    }

    public static void Main()
    {
        // This function is executed when the plugin is selected in the "Run Script..." menu
    }
    
}