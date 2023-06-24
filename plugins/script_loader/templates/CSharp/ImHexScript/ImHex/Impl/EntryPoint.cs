using System.Runtime.InteropServices;

namespace ImHex
{

    public static class Library
    {
        public const string Name = "script_loader.hexplug";
    }


    public class EntryPoint
    {
        [UnmanagedCallersOnly]
        public static void ScriptMain()
        {
            try
            {
                Script.Main();
            } catch(Exception e)
            {
                Console.WriteLine(e.ToString());
            }
        }

    }
}
