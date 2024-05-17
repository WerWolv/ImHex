using System.Reflection;
using System.Runtime.InteropServices;
using System.Runtime.Loader;

namespace ImHex
{

    public class EntryPoint
    {

        private static void Log(string message)
        {
            Console.WriteLine($"[.NET Script] {message}");
        }
        public static int ExecuteScript(IntPtr argument, int argumentLength)
        {
            try
            {
                return ExecuteScript(Marshal.PtrToStringUTF8(argument, argumentLength));
            }
            catch (Exception e)
            {
                Log($"Exception in AssemblyLoader: {e}");
                return 1;
            }
        }

        private static readonly List<string> LoadedPlugins = new();
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
                Log("Failed to get base path");
                return 1;
            }

            // Create a new assembly context
            AssemblyLoadContext? context = new("ScriptDomain_" + basePath, true);

            int result = 0;
            try
            {
                if (type is "LOAD")
                {
                    // If the script has been loaded already, don't do it again
                    if (LoadedPlugins.Contains(path))
                    {
                        return 0;
                    }

                    // Check if the plugin is already loaded
                    LoadedPlugins.Add(path);
                }

                // Load all assemblies in the parent folder
                foreach (var file in Directory.GetFiles(basePath, "*.dll")) {
                    // Skip main Assembly
                    if (new FileInfo(file).Name == "Main.dll")
                    {
                        continue;
                    }

                    // Load the Assembly
                    try
                    {
                        context.LoadFromStream(new MemoryStream(File.ReadAllBytes(file)));
                    }
                    catch (Exception e)
                    {
                        Log($"Failed to load assembly: {file} - {e}");
                    }
                }

                // Load the script assembly
                var assembly = context.LoadFromStream(new MemoryStream(File.ReadAllBytes(path)));

                // Find ImHexLibrary module
                var libraryModule = Array.Find(context.Assemblies.ToArray(), module => module.GetName().Name == "ImHexLibrary");
                if (libraryModule == null)
                {
                    Log("Refusing to load non-ImHex script");
                    return 1;
                }
                else
                {
                    // Load Library type
                    var libraryType = libraryModule.GetType("Library");
                    if (libraryType == null)
                    {
                        Log("Failed to find Library type in ImHexLibrary");
                        return 1;
                    }
                    
                    // Load Initialize function in the Library type
                    var initMethod = libraryType.GetMethod("Initialize", BindingFlags.Static | BindingFlags.Public);
                    if (initMethod == null)
                    {
                        Log("Failed to find Initialize method");
                        return 1;
                    }

                    // Execute it
                    initMethod.Invoke(null, null);
                }
                
                // Find classes derived from IScript
                var entryPointTypes = Array.FindAll(assembly.GetTypes(), t => t.GetInterface("IScript") != null);
                
                if (entryPointTypes.Length == 0)
                {
                    Log("Failed to find Script entrypoint");
                    return 1;
                } else if (entryPointTypes.Length > 1)
                {
                    Log("Found multiple Script entrypoints");
                    return 1;
                }
                
                var entryPointType = entryPointTypes[0];

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
                    // Check if the method exists
                    return entryPointType.GetMethod(methodName, BindingFlags.Static | BindingFlags.Public) != null ? 0 : 1;
                }
                else
                {
                    return 1;
                }
            }
            catch (Exception e)
            {
                Log($"Exception in AssemblyLoader: {e}");
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
