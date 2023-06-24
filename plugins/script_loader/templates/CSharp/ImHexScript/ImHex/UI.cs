#pragma warning disable SYSLIB1054

using System.Drawing;
using System.Runtime.InteropServices;
using System.Text;

namespace ImHex
{
    public class UI
    {
        [DllImport(Library.Name)]
        private static extern void showMessageBoxV1([MarshalAs(UnmanagedType.LPStr)] string message);

        [DllImport(Library.Name)]
        private static extern void showInputTextBoxV1([MarshalAs(UnmanagedType.LPStr)] string title, [MarshalAs(UnmanagedType.LPStr)] string message, StringBuilder buffer, int bufferSize);

        [DllImport(Library.Name)]
        private static extern unsafe void showYesNoQuestionBoxV1([MarshalAs(UnmanagedType.LPStr)] string title, [MarshalAs(UnmanagedType.LPStr)] string message, bool* result);

        public static void ShowMessageBox(string message)
        {
            unsafe
            {
                showMessageBoxV1(message);
            }
        }

        public static bool ShowYesNoQuestionBox(string title, string message)
        {
            unsafe
            {
                bool result = false;
                showYesNoQuestionBoxV1(title, message, &result);
                return result;
            }
        }

        public static string? ShowInputTextBox(string title, string message, int maxSize)
        {
            unsafe
            {
                StringBuilder buffer = new(maxSize);
                showInputTextBoxV1(title, message, buffer, buffer.Capacity);

                if (buffer.Length == 0 || buffer[0] == '\x00')
                    return null;
                else
                    return buffer.ToString();
            }
        }

    }
}
