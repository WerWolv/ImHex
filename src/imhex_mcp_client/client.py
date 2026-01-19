"""
ImHex MCP Client

High-level client for connecting to and interacting with ImHex MCP server.
Provides connection management, tool invocation, and error handling.
"""

import socket
import logging
from typing import Any, Dict, List, Optional
from contextlib import contextmanager

from .protocol import (
    JsonRpcRequest,
    JsonRpcResponse,
    JsonRpcError,
    receive_response_bytes
)


# Configure logging
logger = logging.getLogger(__name__)


class ImHexMCPClient:
    """
    Client for ImHex MCP (Model Context Protocol) server.

    The client connects to ImHex's MCP server on port 19743 and provides
    access to binary analysis tools through JSON-RPC 2.0 protocol.

    Example:
        >>> client = ImHexMCPClient()
        >>> client.connect()
        >>> client.initialize("MyApp", "1.0.0")
        >>> tools = client.list_tools()
        >>> result = client.call_tool("open_file", {"file_path": "/path/to/file.bin"})
        >>> client.disconnect()

    Or using context manager:
        >>> with ImHexMCPClient() as client:
        ...     client.initialize("MyApp", "1.0.0")
        ...     result = client.call_tool("open_file", {"file_path": "/path/to/file.bin"})
    """

    DEFAULT_HOST = '127.0.0.1'
    DEFAULT_PORT = 19743
    DEFAULT_TIMEOUT = 30.0
    PROTOCOL_VERSION = "2025-06-18"

    def __init__(
        self,
        host: str = DEFAULT_HOST,
        port: int = DEFAULT_PORT,
        timeout: float = DEFAULT_TIMEOUT
    ):
        """
        Initialize ImHex MCP client.

        Args:
            host: ImHex MCP server host (default: 127.0.0.1)
            port: ImHex MCP server port (default: 19743)
            timeout: Socket timeout in seconds (default: 30.0)
        """
        self.host = host
        self.port = port
        self.timeout = timeout
        self.sock: Optional[socket.socket] = None
        self.request_id = 0
        self.is_initialized = False
        self.server_info: Dict[str, Any] = {}

    def connect(self) -> None:
        """
        Establish TCP connection to ImHex MCP server.

        Raises:
            ConnectionRefusedError: If server is not running or not accepting connections
            TimeoutError: If connection times out
            OSError: For other socket errors
        """
        if self.sock is not None:
            logger.warning("Already connected, closing existing connection")
            self.disconnect()

        try:
            self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.sock.settimeout(self.timeout)
            self.sock.connect((self.host, self.port))
            logger.info(f"Connected to ImHex MCP server at {self.host}:{self.port}")
        except ConnectionRefusedError:
            logger.error(f"Connection refused to {self.host}:{self.port}")
            logger.error("Make sure ImHex is running and MCP Server is enabled")
            logger.error("Enable it in: Settings > General > MCP Server")
            raise
        except Exception as e:
            logger.error(f"Failed to connect: {e}")
            raise

    def disconnect(self) -> None:
        """Close connection to ImHex MCP server."""
        if self.sock:
            try:
                self.sock.close()
                logger.info("Disconnected from ImHex MCP server")
            except Exception as e:
                logger.warning(f"Error closing connection: {e}")
            finally:
                self.sock = None
                self.is_initialized = False

    def _next_request_id(self) -> int:
        """Generate next request ID."""
        self.request_id += 1
        return self.request_id

    def _send_request(
        self,
        method: str,
        params: Optional[Dict[str, Any]] = None,
        expect_response: bool = True
    ) -> Optional[JsonRpcResponse]:
        """
        Send JSON-RPC request and receive response.

        Args:
            method: JSON-RPC method name
            params: Method parameters
            expect_response: Whether to wait for response (False for notifications)

        Returns:
            JsonRpcResponse if expect_response is True, None otherwise

        Raises:
            ConnectionError: If not connected
            TimeoutError: If request times out
            ValueError: If response contains error
        """
        if not self.sock:
            raise ConnectionError("Not connected to ImHex MCP server")

        # Create request
        if expect_response:
            request = JsonRpcRequest(
                method=method,
                params=params,
                request_id=self._next_request_id()
            )
        else:
            request = JsonRpcRequest.notification(method=method, params=params)

        # Send request
        try:
            request_bytes = request.to_bytes()
            self.sock.sendall(request_bytes)
            logger.debug(f"Sent request: {method}")

            if not expect_response:
                return None

            # Receive response
            response_bytes = receive_response_bytes(self.sock)
            response_json = response_bytes.decode('utf-8')
            response = JsonRpcResponse.from_json(response_json)

            logger.debug(f"Received response for: {method}")

            # Check for errors
            if response.is_error():
                error = response.get_error()
                logger.error(f"JSON-RPC error: {error}")
                raise ValueError(str(error))

            return response

        except socket.timeout:
            logger.error(f"Timeout waiting for response to {method}")
            raise TimeoutError(f"Request timed out after {self.timeout}s")
        except ConnectionError as e:
            logger.error(f"Connection error: {e}")
            self.sock = None
            raise
        except Exception as e:
            logger.error(f"Error in request: {e}")
            raise

    def initialize(
        self,
        client_name: str = "ImHex MCP Python Client",
        client_version: str = "1.0.0"
    ) -> Dict[str, Any]:
        """
        Initialize MCP session with ImHex server.

        Must be called after connect() and before using tools.

        Args:
            client_name: Name of the client application
            client_version: Version of the client application

        Returns:
            Server information dictionary with protocolVersion and serverInfo

        Raises:
            ConnectionError: If not connected
            ValueError: If initialization fails
        """
        params = {
            "protocolVersion": self.PROTOCOL_VERSION,
            "clientInfo": {
                "name": client_name,
                "version": client_version
            }
        }

        response = self._send_request("initialize", params)
        result = response.get_result()  # type: ignore

        # Validate protocol version
        server_protocol = result.get("protocolVersion")
        if server_protocol != self.PROTOCOL_VERSION:
            logger.warning(
                f"Protocol version mismatch: "
                f"client={self.PROTOCOL_VERSION}, server={server_protocol}"
            )

        self.server_info = result.get("serverInfo", {})
        logger.info(
            f"Initialized with ImHex "
            f"{self.server_info.get('name', 'Unknown')} "
            f"v{self.server_info.get('version', 'Unknown')}"
        )

        # Send initialized notification
        self._send_request("notifications/initialized", expect_response=False)
        self.is_initialized = True

        return result

    def list_tools(self) -> List[Dict[str, Any]]:
        """
        List all available MCP tools.

        Returns:
            List of tool dictionaries with name, title, description, and schemas

        Raises:
            ConnectionError: If not connected
            ValueError: If request fails
        """
        response = self._send_request("tools/list")
        result = response.get_result()  # type: ignore

        tools = result.get("tools", [])
        logger.info(f"Found {len(tools)} available tools")

        return tools

    def call_tool(
        self,
        tool_name: str,
        arguments: Optional[Dict[str, Any]] = None
    ) -> Dict[str, Any]:
        """
        Call an MCP tool by name.

        Args:
            tool_name: Name of the tool to call
            arguments: Tool-specific arguments (default: empty dict)

        Returns:
            Tool result as dictionary

        Raises:
            ConnectionError: If not connected
            ValueError: If tool call fails or tool doesn't exist
        """
        if arguments is None:
            arguments = {}

        params = {
            "name": tool_name,
            "arguments": arguments
        }

        response = self._send_request("tools/call", params)
        result = response.get_result()  # type: ignore

        # Extract structured content if available
        if isinstance(result, dict) and "structuredContent" in result:
            return result["structuredContent"]

        return result

    def is_connected(self) -> bool:
        """Check if client is connected to server."""
        return self.sock is not None

    def __enter__(self) -> 'ImHexMCPClient':
        """Context manager entry - connect to server."""
        self.connect()
        return self

    def __exit__(self, exc_type, exc_val, exc_tb) -> None:
        """Context manager exit - disconnect from server."""
        self.disconnect()

    def __repr__(self) -> str:
        status = "connected" if self.is_connected() else "disconnected"
        init = " (initialized)" if self.is_initialized else ""
        return f"ImHexMCPClient({self.host}:{self.port}, {status}{init})"


@contextmanager
def imhex_session(
    client_name: str = "ImHex MCP Python Client",
    client_version: str = "1.0.0",
    host: str = ImHexMCPClient.DEFAULT_HOST,
    port: int = ImHexMCPClient.DEFAULT_PORT
):
    """
    Context manager for ImHex MCP session.

    Automatically connects, initializes, and disconnects.

    Example:
        >>> with imhex_session("MyApp", "1.0.0") as client:
        ...     tools = client.list_tools()
        ...     result = client.call_tool("open_file", {"file_path": "/path/to/file.bin"})

    Args:
        client_name: Name of the client application
        client_version: Version of the client application
        host: ImHex MCP server host
        port: ImHex MCP server port

    Yields:
        Initialized ImHexMCPClient instance
    """
    client = ImHexMCPClient(host=host, port=port)
    try:
        client.connect()
        client.initialize(client_name, client_version)
        yield client
    finally:
        client.disconnect()
