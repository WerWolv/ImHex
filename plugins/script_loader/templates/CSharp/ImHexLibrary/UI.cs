#pragma warning disable SYSLIB1054

using System.Runtime.InteropServices;
using System.Text;

namespace ImHex
{
    public class UI
    {
        private delegate void DrawContentDelegate();

        private static List<Delegate> _registeredDelegates = new();

        [DllImport(Library.Name)]
        private static extern void showMessageBoxV1(byte[] message);

        [DllImport(Library.Name)]
        private static extern void showInputTextBoxV1(byte[] title, byte[] message, StringBuilder buffer, int bufferSize);

        [DllImport(Library.Name)]
        private static extern unsafe void showYesNoQuestionBoxV1(byte[] title, byte[] message, bool* result);
        
        [DllImport(Library.Name)]
        private static extern void showToastV1(byte[] message, UInt32 type);
        
        [DllImport(Library.Name)]
        private static extern IntPtr getImGuiContextV1();
        
        [DllImport(Library.Name)]
        private static extern void registerViewV1(byte[] icon, byte[] name, IntPtr drawFunction);

        public static void ShowMessageBox(string message)
        {
            showMessageBoxV1(Encoding.UTF8.GetBytes(message));
        }

        public static bool ShowYesNoQuestionBox(string title, string message)
        {
            unsafe
            {
                bool result = false;
                showYesNoQuestionBoxV1(Encoding.UTF8.GetBytes(title), Encoding.UTF8.GetBytes(message), &result);
                return result;
            }
        }

        public static string? ShowInputTextBox(string title, string message, int maxSize)
        {
            StringBuilder buffer = new(maxSize);
            showInputTextBoxV1(Encoding.UTF8.GetBytes(title), Encoding.UTF8.GetBytes(message), buffer, buffer.Capacity);

            if (buffer.Length == 0 || buffer[0] == '\x00')
                return null;
            else
                return buffer.ToString();
        }

        public enum ToastType
        {
            Info    = 0,
            Warning = 1,
            Error   = 2
        }

        public static void ShowToast(string message, ToastType type = ToastType.Info)
        {
            showToastV1(Encoding.UTF8.GetBytes(message), (UInt32)type);
        }

        public static IntPtr GetImGuiContext()
        {
            return getImGuiContextV1();
        }

        public static void RegisterView(byte[] icon, string name, Action function)
        {
            _registeredDelegates.Add(new DrawContentDelegate(function));
            registerViewV1(
                icon,
                Encoding.UTF8.GetBytes(name),
                Marshal.GetFunctionPointerForDelegate(_registeredDelegates[^1])
            );
        }

    }
}
