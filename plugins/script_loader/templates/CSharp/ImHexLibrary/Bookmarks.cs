using System.Drawing;
using System.Runtime.InteropServices;
using System.Text;

namespace ImHex
{
    public partial class Bookmarks
    {
        [LibraryImport("ImHex")]
        private static partial void createBookmarkV1(UInt64 address, UInt64 size, UInt32 color, byte[] name, byte[] description);

        public static void CreateBookmark(long address, long size, Color color, string name = "", string description = "")
        {
            createBookmarkV1((UInt64)address, (UInt64)size, (UInt32)(0xA0 << 24 | color.B << 16 | color.G << 8 | color.R), Encoding.UTF8.GetBytes(name), Encoding.UTF8.GetBytes(description));
        }

    }
}
