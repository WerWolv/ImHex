"""
ImHex MCP Client

Python client library for connecting to ImHex MCP (Model Context Protocol) server.
Provides high-level access to ImHex binary analysis tools through a simple API.

Example usage:
    >>> from imhex_mcp_client import ImHexMCPClient, open_file, read_data
    >>>
    >>> # Connect and initialize
    >>> client = ImHexMCPClient()
    >>> client.connect()
    >>> client.initialize("MyApp", "1.0.0")
    >>>
    >>> # Open a file and read data
    >>> ds = open_file(client, "/path/to/firmware.bin")
    >>> data = read_data(client, address=0, size=1024)
    >>> print(f"Read {len(data)} bytes: {data.hex()[:32]}...")
    >>>
    >>> client.disconnect()

Or using context manager:
    >>> from imhex_mcp_client import imhex_session, open_file, read_data
    >>>
    >>> with imhex_session("MyApp", "1.0.0") as client:
    ...     ds = open_file(client, "/path/to/file.bin")
    ...     data = read_data(client, 0, 256)
"""

__version__ = "1.0.0"
__author__ = "ImHex Team"
__license__ = "GPL-2.0"

# Core client
from .client import (
    ImHexMCPClient,
    imhex_session
)

# Protocol types
from .protocol import (
    JsonRpcRequest,
    JsonRpcResponse,
    JsonRpcError,
    JsonRpcErrorCode
)

# Tool wrappers
from .tools import (
    DataSource,
    open_file,
    list_open_data_sources,
    select_data_source,
    read_data,
    execute_pattern_code,
    get_patterns,
    get_pattern_console_content,
    # Convenience functions
    read_file_header,
    analyze_with_pattern,
    hex_dump
)

__all__ = [
    # Client
    "ImHexMCPClient",
    "imhex_session",
    # Protocol
    "JsonRpcRequest",
    "JsonRpcResponse",
    "JsonRpcError",
    "JsonRpcErrorCode",
    # Tools
    "DataSource",
    "open_file",
    "list_open_data_sources",
    "select_data_source",
    "read_data",
    "execute_pattern_code",
    "get_patterns",
    "get_pattern_console_content",
    "read_file_header",
    "analyze_with_pattern",
    "hex_dump",
]
