using System.Reflection;
using System.Runtime.InteropServices;
using System.Runtime.Loader;

namespace ImHex
{

    public class EntryPoint
    {

        private const int ResultSuccess                 = 0x0000_0000;
        private const int ResultError                   = 0x1000_0001;
        private const int ResultMethodNotFound          = 0x1000_0002;
        private const int ResultLoaderError             = 0x1000_0003;
        private const int ResultLoaderInvalidCommand    = 0x1000_0004;

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
                return ResultLoaderError;
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
            var basePath = Path.GetDirectoryName(path);
            if (basePath == null)
            {
                Log("Failed to get base path");
                return ResultError;
            }

            // Create a new assembly context
            AssemblyLoadContext? context = new("ScriptDomain_" + basePath, true);

            int result;
            try
            {
                if (type is "LOAD")
                {
                    // If the script has been loaded already, don't do it again
                    if (LoadedPlugins.Contains(path))
                    {
                        return ResultSuccess;
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
                    return ResultError;
                }
                else
                {
                    // Load Library type
                    var libraryType = libraryModule.GetType("Library");
                    if (libraryType == null)
                    {
                        Log("Failed to find Library type in ImHexLibrary");
                        return ResultError;
                    }
                    
                    // Load Initialize function in the Library type
                    var initMethod = libraryType.GetMethod("Initialize", BindingFlags.Static | BindingFlags.Public);
                    if (initMethod == null)
                    {
                        Log("Failed to find Initialize method");
                        return ResultError;
                    }

                    // Execute it
                    initMethod.Invoke(null, null);
                }
                
                // Find classes derived from IScript
                var entryPointTypes = Array.FindAll(assembly.GetTypes(), t => t.GetInterface("IScript") != null);
                
                if (entryPointTypes.Length == 0)
                {
                    Log("Failed to find Script entrypoint");
                    return ResultError;
                } else if (entryPointTypes.Length > 1)
                {
                    Log("Found multiple Script entrypoints");
                    return ResultError;
                }
                
                var entryPointType = entryPointTypes[0];

                if (type is "EXEC" or "LOAD")
                {
                    // Load the function
                    var method = entryPointType.GetMethod(methodName, BindingFlags.Static | BindingFlags.Public);
                    if (method == null)
                    {
                        return ResultMethodNotFound;
                    }

                    // Execute it
                    var returnValue = method.Invoke(null, null);
                    switch (returnValue)
                    {
                        case null:
                            result = ResultSuccess;
                            break;
                        case int intValue:
                            result = intValue;
                            break;
                        case uint intValue:
                            result = (int)intValue;
                            break;
                        default:
                            result = ResultError;
                            Log($"Invalid return value from script: {returnValue.GetType().Name} {{{returnValue}}}");
                            break;
                    }
                }
                else if (type == "CHECK")
                {
                    // Check if the method exists
                    return entryPointType.GetMethod(methodName, BindingFlags.Static | BindingFlags.Public) != null ? 0 : 1;
                }
                else
                {
                    return ResultLoaderInvalidCommand;
                }
            }
            catch (Exception e)
            {
                Log($"Exception in AssemblyLoader: {e}");
                return ResultLoaderError;
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
