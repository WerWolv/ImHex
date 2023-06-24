using ImHex;
using System.Drawing;

class Script
{
    public static void Main()
    {
        string? result = UI.ShowInputTextBox("Title", "Message", 100);

        if (result != null)
            UI.ShowMessageBox("Hello World Result: " + result);
    }
}