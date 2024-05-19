using System.Runtime.InteropServices;
using System.Text;

namespace ImHex
{
    public interface IProvider
    {
        void readRaw(UInt64 address, IntPtr buffer, UInt64 size)
        {
            unsafe
            {
                Span<byte> data = new(buffer.ToPointer(), (int)size);
                read(address, data);
            }
        }
        
        void writeRaw(UInt64 address, IntPtr buffer, UInt64 size)
        {
            unsafe
            {
                ReadOnlySpan<byte> data = new(buffer.ToPointer(), (int)size);
                write(address, data);
            }
        }
        
        void read(UInt64 address, Span<byte> data);
        void write(UInt64 address, ReadOnlySpan<byte> data);

        UInt64 getSize();

        string getTypeName();
        string getName();
    }
    public partial class Memory
    {
        private static readonly List<IProvider> RegisteredProviders = new();
        private static readonly List<Delegate> RegisteredDelegates = new();

        private delegate void DataAccessDelegate(UInt64 address, IntPtr buffer, UInt64 size);
        private delegate UInt64 GetSizeDelegate();
        
        [LibraryImport("ImHex")]
        private static partial void readMemoryV1(UInt64 address, UInt64 size, IntPtr buffer);

        [LibraryImport("ImHex")]
        private static partial void writeMemoryV1(UInt64 address, UInt64 size, IntPtr buffer);

        [LibraryImport("ImHex")]
        private static partial int getSelectionV1(IntPtr start, IntPtr end);
        
        [LibraryImport("ImHex")]
        private static partial void registerProviderV1(byte[] typeName, byte[] name, IntPtr readFunction, IntPtr writeFunction, IntPtr getSizeFunction);


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
                if (getSelectionV1((nint)(&start), (nint)(&end)) == 0)
                {
                    return null;
                }

                return (start, end);
            }
        }
        
        public static void RegisterProvider<T>() where T : IProvider, new()
        {
            RegisteredProviders.Add(new T());
            
            ref var provider = ref CollectionsMarshal.AsSpan(RegisteredProviders)[^1];
            
            RegisteredDelegates.Add(new DataAccessDelegate(provider.readRaw));
            RegisteredDelegates.Add(new DataAccessDelegate(provider.writeRaw));
            RegisteredDelegates.Add(new GetSizeDelegate(provider.getSize));
            
            registerProviderV1(
                Encoding.UTF8.GetBytes(provider.getTypeName()), 
                Encoding.UTF8.GetBytes(provider.getName()), 
                Marshal.GetFunctionPointerForDelegate(RegisteredDelegates[^3]), 
                Marshal.GetFunctionPointerForDelegate(RegisteredDelegates[^2]),
                Marshal.GetFunctionPointerForDelegate(RegisteredDelegates[^1])
            );
        }

    }
}
