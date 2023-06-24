#pragma warning disable SYSLIB1054

using System.Drawing;
using System.Runtime.InteropServices;

namespace ImHex
{
    public class Memory
    {
        [DllImport(Library.Name)]
        private static extern void readMemoryV1(UInt64 address, UInt64 size, IntPtr buffer);

        [DllImport(Library.Name)]
        private static extern void writeMemoryV1(UInt64 address, UInt64 size, IntPtr buffer);

        [DllImport(Library.Name)]
        private static extern bool getSelectionV1(IntPtr start, IntPtr end);


        public static byte[] Read(ulong address, ulong size)
        {
            byte[] bytes = new byte[size];

            unsafe
            {
                fixed (byte* buffer = bytes)
                {
                    readMemoryV1(address, size, (IntPtr)buffer);
                }
            }


            return bytes;
        }

        public static void Write(ulong address, byte[] bytes)
        {
            unsafe
            {
                fixed (byte* buffer = bytes)
                {
                    writeMemoryV1(address, (UInt64)bytes.Length, (IntPtr)buffer);
                }
            }
        }

        public static (UInt64, UInt64)? GetSelection()
        {
            unsafe
            {
                UInt64 start = 0, end = 0;
                if (!getSelectionV1((nint)(&start), (nint)(&end)))
                {
                    return null;
                }

                return (start, end);
            }
        }

    }
}
