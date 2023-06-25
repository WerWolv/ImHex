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
                Console.WriteLine("[.NET Script] Exception in AssemblyLoader: " + e.Message);
                return 1;
            }
        }


        private static bool ExecuteScript(string path)
        {
            AssemblyLoadContext context = new("ScriptDomain", true);

            try
            {
                var assembly = context.LoadFromAssemblyPath(path);

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
                Console.WriteLine("[.NET Script] Exception in AssemblyLoader: " + e.Message);
                return false;
            }
            finally
            {
                context.Unload();
            }

            return true;
        }

    }
}
