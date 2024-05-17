using System.Runtime.InteropServices;
using System.Text;

#pragma warning disable SYSLIB1054

namespace ImHex
{
    public static partial class Logger
    {
        [LibraryImport("ImHex")]
        private static partial void logPrintV1(byte[] message);
        
        [LibraryImport("ImHex")]
        private static partial void logPrintlnV1(byte[] message);
        
        [LibraryImport("ImHex")]
        private static partial void logDebugV1(byte[] message);
        
        [LibraryImport("ImHex")]
        private static partial void logInfoV1(byte[] message);
        
        [LibraryImport("ImHex")]
        private static partial void logWarnV1(byte[] message);
        
        [LibraryImport("ImHex")]
        private static partial void logErrorV1(byte[] message);
        
        [LibraryImport("ImHex")]
        private static partial void logFatalV1(byte[] message);
        

        public static void Debug(string message)
        {
            logDebugV1(Encoding.UTF8.GetBytes(message));
        }
        
        public static void Info(string message)
        {
            logInfoV1(Encoding.UTF8.GetBytes(message));
        }
        
        public static void Warn(string message)
        {
            logWarnV1(Encoding.UTF8.GetBytes(message));
        }
        
        public static void Error(string message)
        {
            logErrorV1(Encoding.UTF8.GetBytes(message));
        }
        
        public static void Fatal(string message)
        {
            logFatalV1(Encoding.UTF8.GetBytes(message));
        }
        
        
        private class LoggerWriter : TextWriter
        {
            public override Encoding Encoding => Encoding.UTF8;

            public override void Write(string? value)
            {
                logPrintV1(Encoding.GetBytes(value ?? string.Empty));
            }
            
            public override void WriteLine(string? value)
            {
                logPrintlnV1(Encoding.GetBytes(value ?? string.Empty));
            }
        }  
        
        public static void RedirectConsole()
        {
            Console.SetOut(new LoggerWriter());
        }
    }
}