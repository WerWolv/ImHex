using System.Reflection;
using System.Runtime.InteropServices;
using System.Runtime.Loader;

namespace ImHex
{

    public class EntryPoint
    {

        public static int ExecuteScript(IntPtr arg, int argLength)
        {
            try
            {
                return ExecuteScript(Marshal.PtrToStringUTF8(arg, argLength)) ? 0 : 1;
            }
            catch (Exception e)
            {
                Console.WriteLine("[.NET Script] Exception in AssemblyLoader: " + e.ToString());
                return 1;
            }
        }


        private static bool ExecuteScript(string path)
        {
            string? basePath = Path.GetDirectoryName(path);
            if (basePath == null)
            {
                Console.WriteLine("[.NET Script] Failed to get base path");
                return false;
            }

            AssemblyLoadContext? context = new("ScriptDomain_" + basePath, true);

            try
            {
                foreach (var file in Directory.GetFiles(basePath, "*.dll"))
                {
                    context.LoadFromStream(new MemoryStream(File.ReadAllBytes(file)));
                }

                var assembly = context.LoadFromStream(new MemoryStream(File.ReadAllBytes(path)));

                var entryPointType = assembly.GetType("Script");
                if (entryPointType == null)
                {
                    Console.WriteLine("[.NET Script] Failed to find Script type");
                    return false;
                }

                var entryPointMethod = entryPointType.GetMethod("Main", BindingFlags.Static | BindingFlags.Public);
                if (entryPointMethod == null)
                {
                    Console.WriteLine("[.NET Script] Failed to find ScriptMain method");
                    return false;
                }

                entryPointMethod.Invoke(null, null);
            }
            catch (Exception e)
            {
                Console.WriteLine("[.NET Script] Exception in AssemblyLoader: " + e.ToString());
                return false;
            }
            finally
            {
                context.Unload();
                context = null;

                for (int i = 0; i < 10; i++)
                {
                    GC.Collect();
                    GC.WaitForPendingFinalizers();
                }
            }

            return true;
        }

    }
}
