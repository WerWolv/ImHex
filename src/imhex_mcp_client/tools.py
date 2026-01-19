"""
ImHex MCP Tools - High-level wrappers

Provides user-friendly wrappers for all 7 MCP tools available in ImHex.
These functions handle parameter validation, type conversion, and result parsing.
"""

import base64
import logging
from typing import Dict, List, Any, Optional

from .client import ImHexMCPClient


logger = logging.getLogger(__name__)


# Data Source representation
class DataSource:
    """Represents an open data source (file) in ImHex"""

    def __init__(self, data: Dict[str, Any]):
        self.name: str = data.get("name", "")
        self.type: str = data.get("type", "")
        self.size: int = data.get("size", 0)
        self.is_writable: bool = data.get("is_writable", False)
        self.handle: int = data.get("handle", -1)

    def __repr__(self) -> str:
        writable = "writable" if self.is_writable else "read-only"
        return f"DataSource(name='{self.name}', size={self.size} bytes, handle={self.handle}, {writable})"

    def __str__(self) -> str:
        return self.name


# Tool wrapper functions

def open_file(client: ImHexMCPClient, file_path: str) -> DataSource:
    """
    Open a file in ImHex.

    This is the first operation that must be performed before using other tools.
    The file remains open until closed by the user in ImHex.

    Args:
        client: Connected ImHexMCPClient instance
        file_path: Absolute path to the file to open

    Returns:
        DataSource object representing the opened file

    Raises:
        ValueError: If file cannot be opened or path is invalid

    Example:
        >>> client = ImHexMCPClient()
        >>> client.connect()
        >>> client.initialize()
        >>> ds = open_file(client, "/path/to/firmware.bin")
        >>> print(f"Opened {ds.name}, size: {ds.size} bytes")
    """
    result = client.call_tool("open_file", {"file_path": file_path})
    return DataSource(result)


def list_open_data_sources(client: ImHexMCPClient) -> List[DataSource]:
    """
    List all currently open data sources in ImHex.

    Returns:
        List of DataSource objects

    Example:
        >>> sources = list_open_data_sources(client)
        >>> for ds in sources:
        ...     print(f"{ds.handle}: {ds.name} ({ds.size} bytes)")
    """
    result = client.call_tool("list_open_data_sources", {})
    data_sources = result.get("data_sources", [])
    return [DataSource(ds) for ds in data_sources]


def select_data_source(client: ImHexMCPClient, handle: int) -> int:
    """
    Select a data source by its handle to use in subsequent operations.

    All operations like read_data, execute_pattern_code work on the
    currently selected data source.

    Args:
        client: Connected ImHexMCPClient instance
        handle: Handle of the data source to select

    Returns:
        Handle of the selected data source (same as input if successful,
        previous handle if failed)

    Example:
        >>> select_data_source(client, 2)
        2
    """
    result = client.call_tool("select_data_source", {"handle": handle})
    return result.get("selected_handle", -1)


def read_data(
    client: ImHexMCPClient,
    address: int,
    size: int,
    decode: bool = True
) -> bytes:
    """
    Read binary data from the currently selected data source.

    Args:
        client: Connected ImHexMCPClient instance
        address: Starting address (offset) to read from
        size: Number of bytes to read (max 16 MiB)
        decode: Whether to decode base64 to bytes (default: True)

    Returns:
        Raw bytes if decode=True, base64 string if decode=False

    Raises:
        ValueError: If size exceeds 16 MiB or address is invalid

    Example:
        >>> data = read_data(client, address=0, size=256)
        >>> print(f"Read {len(data)} bytes")
        >>> print(data.hex()[:32])  # First 16 bytes as hex
    """
    MAX_SIZE = 16 * 1024 * 1024  # 16 MiB

    if size > MAX_SIZE:
        logger.warning(f"Size {size} exceeds maximum {MAX_SIZE}, will be capped by server")

    result = client.call_tool("read_data", {
        "address": address,
        "size": size
    })

    data_b64 = result.get("data", "")
    actual_size = result.get("data_size", 0)

    if actual_size != size:
        logger.warning(f"Requested {size} bytes but got {actual_size}")

    if decode:
        return base64.b64decode(data_b64)
    else:
        return data_b64  # type: ignore


def execute_pattern_code(
    client: ImHexMCPClient,
    source_code: str
) -> int:
    """
    Execute Pattern Language code on the currently selected data source.

    The Pattern Language is a C-like DSL for parsing binary structures.
    After execution, use get_patterns() to retrieve results or
    get_pattern_console_content() to read output/errors.

    Args:
        client: Connected ImHexMCPClient instance
        source_code: Pattern Language source code

    Returns:
        Result code (0 = success, non-zero = error)

    Example:
        >>> pattern = '''
        ... struct Header {
        ...     u32 magic;
        ...     u32 version;
        ... };
        ... Header header @ 0x00;
        ... '''
        >>> result_code = execute_pattern_code(client, pattern)
        >>> if result_code == 0:
        ...     patterns = get_patterns(client)
        ... else:
        ...     console = get_pattern_console_content(client)
        ...     print(f"Error: {console}")
    """
    result = client.call_tool("execute_pattern_code", {
        "source_code": source_code
    })

    result_code = result.get("result_code", -1)

    if result_code == 0:
        logger.info("Pattern executed successfully")
    else:
        logger.warning(f"Pattern execution failed with code {result_code}")

    return result_code


def get_patterns(client: ImHexMCPClient) -> Dict[str, Any]:
    """
    Retrieve patterns generated by the last successful Pattern Language execution.

    Only returns patterns if the last execute_pattern_code() call succeeded
    (result_code was 0).

    Returns:
        Dictionary containing the parsed patterns as structured data

    Example:
        >>> patterns = get_patterns(client)
        >>> print(patterns['header']['magic'])
        0x464C457F
    """
    result = client.call_tool("get_patterns", {})
    return result.get("patterns", {})


def get_pattern_console_content(client: ImHexMCPClient) -> str:
    """
    Get console output from Pattern Language execution.

    Returns output generated by std::print() calls in pattern code,
    or error messages if execution failed.

    Returns:
        Console output as string

    Example:
        >>> console = get_pattern_console_content(client)
        >>> print(console)
        Pattern executed successfully
        Found 42 entries
    """
    result = client.call_tool("get_pattern_console_content", {})
    return result.get("content", "")


# Convenience functions

def read_file_header(
    client: ImHexMCPClient,
    file_path: str,
    header_size: int = 512
) -> bytes:
    """
    Open a file and read its header in one operation.

    Args:
        client: Connected ImHexMCPClient instance
        file_path: Path to the file
        header_size: Number of bytes to read from start (default: 512)

    Returns:
        Header bytes

    Example:
        >>> header = read_file_header(client, "/path/to/file.bin", 256)
        >>> magic = header[:4]
        >>> print(f"Magic: {magic.hex()}")
    """
    ds = open_file(client, file_path)
    select_data_source(client, ds.handle)
    return read_data(client, address=0, size=header_size)


def analyze_with_pattern(
    client: ImHexMCPClient,
    file_path: str,
    pattern_code: str
) -> Dict[str, Any]:
    """
    Open a file and analyze it with Pattern Language in one operation.

    Args:
        client: Connected ImHexMCPClient instance
        file_path: Path to the file
        pattern_code: Pattern Language source code

    Returns:
        Dictionary with patterns if successful, or console output if error

    Example:
        >>> pattern = '''
        ... struct ELFHeader {
        ...     char magic[4];
        ...     u8 class;
        ...     u8 endian;
        ... };
        ... ELFHeader elf @ 0x00;
        ... '''
        >>> result = analyze_with_pattern(client, "/bin/ls", pattern)
        >>> if 'elf' in result:
        ...     print(f"ELF class: {result['elf']['class']}")
    """
    ds = open_file(client, file_path)
    select_data_source(client, ds.handle)

    result_code = execute_pattern_code(client, pattern_code)

    if result_code == 0:
        return get_patterns(client)
    else:
        console = get_pattern_console_content(client)
        return {"error": True, "console": console, "result_code": result_code}


def hex_dump(
    client: ImHexMCPClient,
    address: int,
    size: int,
    width: int = 16
) -> str:
    """
    Read data and format as hexdump.

    Args:
        client: Connected ImHexMCPClient instance
        address: Starting address
        size: Number of bytes to read
        width: Number of bytes per line (default: 16)

    Returns:
        Formatted hexdump string

    Example:
        >>> dump = hex_dump(client, 0, 256)
        >>> print(dump)
        00000000: 7f 45 4c 46 02 01 01 00 00 00 00 00 00 00 00 00  .ELF............
        00000010: 02 00 3e 00 01 00 00 00 ...
    """
    data = read_data(client, address, size)

    lines = []
    for i in range(0, len(data), width):
        chunk = data[i:i + width]
        offset = address + i

        # Hex representation
        hex_part = ' '.join(f'{b:02x}' for b in chunk)
        hex_part = hex_part.ljust(width * 3 - 1)

        # ASCII representation
        ascii_part = ''.join(chr(b) if 32 <= b < 127 else '.' for b in chunk)

        lines.append(f"{offset:08x}: {hex_part}  {ascii_part}")

    return '\n'.join(lines)
