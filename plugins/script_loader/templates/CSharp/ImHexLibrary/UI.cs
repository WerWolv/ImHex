using System.Runtime.InteropServices;
using System.Text;

namespace ImHex
{
    public partial class UI
    {
        private delegate void DrawContentDelegate();
        private delegate void ActionDelegate();

        private static readonly List<Delegate> RegisteredDelegates = new();

        [LibraryImport("ImHex")]
        private static partial void showMessageBoxV1(byte[] message);

        [LibraryImport("ImHex")]
        private static unsafe partial void showInputTextBoxV1(byte[] title, byte[] message, IntPtr buffer, int bufferSize);

        [LibraryImport("ImHex")]
        private static unsafe partial void showYesNoQuestionBoxV1(byte[] title, byte[] message, bool* result);
        
        [LibraryImport("ImHex")]
        private static partial void showToastV1(byte[] message, UInt32 type);
        
        [LibraryImport("ImHex")]
        private static partial IntPtr getImGuiContextV1();
        
        [LibraryImport("ImHex")]
        private static partial void registerViewV1(byte[] icon, byte[] name, IntPtr drawFunction);

        [LibraryImport("ImHex")]
        private static partial void addMenuItemV1(byte[] icon, byte[] menuName, byte[] itemName, IntPtr drawFunction);

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
            var buffer = new byte[maxSize];
            GCHandle pinnedArray = GCHandle.Alloc(buffer, GCHandleType.Pinned);
            showInputTextBoxV1(Encoding.UTF8.GetBytes(title), Encoding.UTF8.GetBytes(message), pinnedArray.AddrOfPinnedObject(), maxSize);
            pinnedArray.Free();

            if (buffer.Length == 0 || buffer[0] == '\x00')
            {
                return null;
            }
            
            return Encoding.UTF8.GetString(buffer);
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
            RegisteredDelegates.Add(new DrawContentDelegate(function));
            registerViewV1(
                icon,
                Encoding.UTF8.GetBytes(name),
                Marshal.GetFunctionPointerForDelegate(RegisteredDelegates[^1])
            );
        }

        public static void AddMenuItem(byte[] icon, string menuName, string itemName, Action function)
        {
            RegisteredDelegates.Add(new ActionDelegate(function));
            addMenuItemV1(
                icon,
                Encoding.UTF8.GetBytes(menuName),
                Encoding.UTF8.GetBytes(itemName),
                Marshal.GetFunctionPointerForDelegate(RegisteredDelegates[^1])
            );
        }

    }
}
