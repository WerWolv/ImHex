using System.Reflection;
using System.Runtime.InteropServices;
using System.Runtime.InteropServices.ComTypes;
using System.Runtime.Loader;

namespace ImHex
{

    public class EntryPoint
    {

        public static int ExecuteScript(IntPtr arg, int argLength)
        {
            try
            {
                return ExecuteScript(Marshal.PtrToStringUTF8(arg, argLength));
            }
            catch (Exception e)
            {
                Console.WriteLine("[.NET Script] Exception in AssemblyLoader: " + e.ToString());
                return 1;
            }
        }

        private static List<string> loadedPlugins = new();
        private static int ExecuteScript(string args)
        {
            // Parse input in the form of "execType||path"
            var splitArgs = args.Split("||");
            var type        = splitArgs[0];
            var methodName  = splitArgs[1];
            var path        = splitArgs[2];

            // Get the parent folder of the passed path
            string? basePath = Path.GetDirectoryName(path);
            if (basePath == null)
            {
                Console.WriteLine("[.NET Script] Failed to get base path");
                return 1;
            }

            // Create a new assembly context
            AssemblyLoadContext? context = new("ScriptDomain_" + basePath, true);

            int result = 0;
            try
            {
                if (type is "LOAD")
                {
                    if (loadedPlugins.Contains(path))
                    {
                        return 0;
                    }

                    // Check if the plugin is already loaded
                    loadedPlugins.Add(path);
                }

                // Load all assemblies in the parent folder
                foreach (var file in Directory.GetFiles(basePath, "*.dll"))
                {
                    // Skip main Assembly
                    if (file.EndsWith("Main.dll"))
                    {
                        continue;
                    }
                    
                    context.LoadFromStream(new MemoryStream(File.ReadAllBytes(file)));
                }

                // Load the script assembly
                var assembly = context.LoadFromStream(new MemoryStream(File.ReadAllBytes(path)));

                // Find a class named "Script"
                var entryPointType = assembly.GetType("Script");
                if (entryPointType == null)
                {
                    Console.WriteLine("[.NET Script] Failed to find Script type");
                    return 1;
                }

                if (type is "EXEC" or "LOAD")
                {
                    // Load the function
                    var method = entryPointType.GetMethod(methodName, BindingFlags.Static | BindingFlags.Public);
                    if (method == null)
                    {
                        return 2;
                    }

                    // Execute it
                    method.Invoke(null, null);   
                }
                else if (type == "CHECK")
                {
                    return entryPointType.GetMethod(methodName, BindingFlags.Static | BindingFlags.Public) != null ? 0 : 1;
                }
                else
                {
                    return 1;
                }
            }
            catch (Exception e)
            {
                Console.WriteLine("[.NET Script] Exception in AssemblyLoader: " + e.ToString());
                return 3;
            }
            finally
            {
                if (type != "LOAD")
                {
                    // Unload all assemblies associated with this script
                    context.Unload();
                    context = null;

                    // Run the garbage collector multiple times to make sure that the
                    // assemblies are unloaded for sure
                    for (int i = 0; i < 10; i++)
                    {
                        GC.Collect();
                        GC.WaitForPendingFinalizers();
                    }
                }
            }

            return result;
        }

    }
}
