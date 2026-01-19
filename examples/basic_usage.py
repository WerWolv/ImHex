#!/usr/bin/env python3
"""
Basic usage examples for ImHex MCP Client

This file demonstrates common use cases for the ImHex MCP Python client.
"""

import sys
from imhex_mcp_client import (
    ImHexMCPClient,
    imhex_session,
    open_file,
    list_open_data_sources,
    select_data_source,
    read_data,
    execute_pattern_code,
    get_patterns,
    get_pattern_console_content,
    read_file_header,
    analyze_with_pattern,
    hex_dump
)


def example_1_basic_connection():
    """Example 1: Basic connection and file opening"""
    print("=" * 60)
    print("Example 1: Basic Connection")
    print("=" * 60)

    client = ImHexMCPClient()

    try:
        # Connect
        client.connect()
        print("✓ Connected to ImHex MCP server")

        # Initialize
        client.initialize("Example Script", "1.0.0")
        print("✓ Initialized MCP session")

        # List available tools
        tools = client.list_tools()
        print(f"✓ Found {len(tools)} available tools:")
        for tool in tools:
            print(f"  - {tool['name']}")

    finally:
        client.disconnect()
        print("✓ Disconnected")

    print()


def example_2_context_manager():
    """Example 2: Using context manager"""
    print("=" * 60)
    print("Example 2: Context Manager")
    print("=" * 60)

    # Automatically connects, initializes, and disconnects
    with imhex_session("Example Script", "1.0.0") as client:
        print("✓ Connected via context manager")

        # Work with client here
        tools = client.list_tools()
        print(f"✓ {len(tools)} tools available")

    print("✓ Auto-disconnected")
    print()


def example_3_read_file(file_path: str):
    """Example 3: Opening and reading a file"""
    print("=" * 60)
    print(f"Example 3: Reading File: {file_path}")
    print("=" * 60)

    with imhex_session() as client:
        # Open file
        ds = open_file(client, file_path)
        print(f"✓ Opened: {ds.name}")
        print(f"  Size: {ds.size} bytes")
        print(f"  Type: {ds.type}")
        print(f"  Writable: {ds.is_writable}")
        print(f"  Handle: {ds.handle}")

        # Read first 64 bytes
        data = read_data(client, address=0, size=64)
        print(f"\n✓ Read {len(data)} bytes")
        print(f"  Magic bytes: {data[:4].hex()}")
        print(f"  First 16 bytes: {data[:16].hex()}")

    print()


def example_4_hex_dump(file_path: str):
    """Example 4: Hex dump visualization"""
    print("=" * 60)
    print(f"Example 4: Hex Dump: {file_path}")
    print("=" * 60)

    with imhex_session() as client:
        open_file(client, file_path)

        # Generate hex dump of first 256 bytes
        dump = hex_dump(client, address=0, size=256, width=16)
        print(dump)

    print()


def example_5_pattern_language(file_path: str):
    """Example 5: Using Pattern Language"""
    print("=" * 60)
    print(f"Example 5: Pattern Language Analysis: {file_path}")
    print("=" * 60)

    # Define a simple pattern
    pattern_code = """
struct FileHeader {
    char magic[4];
    u32 version;
    u32 file_size;
    u32 reserved;
};

FileHeader header @ 0x00;

std::print("Magic: {}", header.magic);
std::print("Version: {}", header.version);
std::print("Size: {} bytes", header.file_size);
"""

    with imhex_session() as client:
        open_file(client, file_path)

        # Execute pattern
        result_code = execute_pattern_code(client, pattern_code)

        if result_code == 0:
            print("✓ Pattern executed successfully\n")

            # Get console output
            console = get_pattern_console_content(client)
            print("Console Output:")
            print(console)

            # Get generated patterns
            patterns = get_patterns(client)
            print("\nGenerated Patterns:")
            print(patterns)
        else:
            print(f"✗ Pattern execution failed (code {result_code})\n")
            console = get_pattern_console_content(client)
            print("Error Output:")
            print(console)

    print()


def example_6_elf_analysis(elf_file: str):
    """Example 6: ELF file analysis with Pattern Language"""
    print("=" * 60)
    print(f"Example 6: ELF Analysis: {elf_file}")
    print("=" * 60)

    elf_pattern = """
struct ELFHeader {
    char magic[4];
    u8 class;         // 1 = 32-bit, 2 = 64-bit
    u8 endianness;    // 1 = little, 2 = big
    u8 version;
    u8 os_abi;
    u64 padding;
    u16 type;
    u16 machine;
    u32 version2;
    u64 entry_point;
};

ELFHeader elf @ 0x00;

std::print("ELF Magic: {}", elf.magic);
std::print("Class: {} ({})", elf.class, elf.class == 1 ? "32-bit" : "64-bit");
std::print("Endianness: {} ({})", elf.endianness, elf.endianness == 1 ? "Little" : "Big");
std::print("Machine: 0x{:04X}", elf.machine);
std::print("Entry Point: 0x{:016X}", elf.entry_point);
"""

    result = analyze_with_pattern(client=None, file_path=elf_file, pattern_code=elf_pattern)

    with imhex_session() as client:
        result = analyze_with_pattern(client, elf_file, elf_pattern)

        if 'error' in result:
            print(f"✗ Analysis failed")
            print(result['console'])
        else:
            print("✓ ELF Analysis Complete\n")
            elf = result.get('elf', {})

            if elf:
                print(f"Magic: {elf.get('magic', 'N/A')}")
                print(f"Class: {elf.get('class', 'N/A')}")
                print(f"Endianness: {elf.get('endianness', 'N/A')}")
                print(f"Machine: 0x{elf.get('machine', 0):04X}")
                print(f"Entry Point: 0x{elf.get('entry_point', 0):016X}")

    print()


def example_7_multiple_files():
    """Example 7: Working with multiple files"""
    print("=" * 60)
    print("Example 7: Multiple Files")
    print("=" * 60)

    with imhex_session() as client:
        # List initially open files
        sources = list_open_data_sources(client)
        print(f"Initially open: {len(sources)} files")

        if len(sources) > 0:
            print("\nOpen Data Sources:")
            for ds in sources:
                print(f"  [{ds.handle}] {ds.name} - {ds.size} bytes")

            # Select each and read header
            for ds in sources:
                print(f"\nReading from {ds.name}:")
                select_data_source(client, ds.handle)
                header = read_data(client, 0, 16)
                print(f"  Magic: {header[:4].hex()}")
        else:
            print("  (No files currently open in ImHex)")

    print()


def example_8_file_type_detection(file_path: str):
    """Example 8: Simple file type detection"""
    print("=" * 60)
    print(f"Example 8: File Type Detection: {file_path}")
    print("=" * 60)

    with imhex_session() as client:
        header = read_file_header(client, file_path, header_size=16)

        # Simple magic number detection
        file_types = {
            b'\x7fELF': "ELF executable",
            b'MZ': "DOS/Windows executable",
            b'\x89PNG': "PNG image",
            b'GIF8': "GIF image",
            b'\xff\xd8\xff': "JPEG image",
            b'PK\x03\x04': "ZIP archive",
            b'\x1f\x8b': "GZIP compressed",
            b'BM': "BMP image",
            b'RIFF': "RIFF container (AVI/WAV)",
            b'%PDF': "PDF document",
        }

        detected = None
        for magic, file_type in file_types.items():
            if header.startswith(magic):
                detected = file_type
                break

        if detected:
            print(f"✓ Detected: {detected}")
        else:
            print(f"? Unknown file type")

        print(f"  Magic bytes: {header[:4].hex()}")

    print()


def main():
    """Main function - runs all examples"""
    print("\n" + "=" * 60)
    print("ImHex MCP Client - Usage Examples")
    print("=" * 60 + "\n")

    # Example 1: Basic connection
    example_1_basic_connection()

    # Example 2: Context manager
    example_2_context_manager()

    # Get a test file from command line or use default
    if len(sys.argv) > 1:
        test_file = sys.argv[1]
    else:
        print("Note: Pass a file path as argument to test file operations")
        print("Usage: python basic_usage.py /path/to/file.bin\n")
        return

    # Example 3: Read file
    example_3_read_file(test_file)

    # Example 4: Hex dump
    example_4_hex_dump(test_file)

    # Example 5: Pattern Language
    example_5_pattern_language(test_file)

    # Example 6: ELF analysis (if file is ELF)
    with open(test_file, 'rb') as f:
        magic = f.read(4)
        if magic == b'\x7fELF':
            example_6_elf_analysis(test_file)

    # Example 7: Multiple files
    example_7_multiple_files()

    # Example 8: File type detection
    example_8_file_type_detection(test_file)

    print("=" * 60)
    print("All examples completed!")
    print("=" * 60)


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print("\n\nInterrupted by user")
        sys.exit(0)
    except Exception as e:
        print(f"\n\nError: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)
