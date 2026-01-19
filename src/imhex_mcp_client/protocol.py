"""
JSON-RPC 2.0 Protocol Implementation for ImHex MCP Client

This module implements the JSON-RPC 2.0 protocol used by ImHex MCP server.
Handles request serialization, response parsing, and error handling.
"""

from typing import Any, Dict, Optional, Union
from dataclasses import dataclass
from enum import IntEnum
import json


class JsonRpcErrorCode(IntEnum):
    """Standard JSON-RPC 2.0 error codes"""
    PARSE_ERROR = -32700
    INVALID_REQUEST = -32600
    METHOD_NOT_FOUND = -32601
    INVALID_PARAMS = -32602
    INTERNAL_ERROR = -32603


@dataclass
class JsonRpcError:
    """Represents a JSON-RPC 2.0 error"""
    code: int
    message: str
    data: Optional[Any] = None

    def __str__(self) -> str:
        if self.data:
            return f"JSON-RPC Error {self.code}: {self.message} (data: {self.data})"
        return f"JSON-RPC Error {self.code}: {self.message}"


class JsonRpcRequest:
    """
    Represents a JSON-RPC 2.0 request.

    Examples:
        >>> req = JsonRpcRequest(method="initialize", params={"protocolVersion": "2025-06-18"}, request_id=1)
        >>> req.to_json()
        '{"jsonrpc": "2.0", "id": 1, "method": "initialize", "params": {...}}'

        >>> notification = JsonRpcRequest.notification("notifications/initialized")
        >>> notification.to_json()
        '{"jsonrpc": "2.0", "method": "notifications/initialized"}'
    """

    def __init__(
        self,
        method: str,
        params: Optional[Union[Dict[str, Any], list]] = None,
        request_id: Optional[Union[int, str]] = None
    ):
        """
        Initialize a JSON-RPC 2.0 request.

        Args:
            method: The method name to call
            params: Parameters for the method (dict or list)
            request_id: Request ID for matching responses (None for notifications)
        """
        self.method = method
        self.params = params
        self.request_id = request_id

    @classmethod
    def notification(cls, method: str, params: Optional[Union[Dict[str, Any], list]] = None) -> 'JsonRpcRequest':
        """
        Create a notification (request without ID that expects no response).

        Args:
            method: The notification method name
            params: Parameters for the notification

        Returns:
            JsonRpcRequest with no ID
        """
        return cls(method=method, params=params, request_id=None)

    def to_dict(self) -> Dict[str, Any]:
        """
        Convert request to dictionary.

        Returns:
            Dictionary representation of the request
        """
        request_dict: Dict[str, Any] = {
            "jsonrpc": "2.0",
            "method": self.method
        }

        if self.request_id is not None:
            request_dict["id"] = self.request_id

        if self.params is not None:
            request_dict["params"] = self.params

        return request_dict

    def to_json(self) -> str:
        """
        Serialize request to JSON string.

        Returns:
            JSON string representation
        """
        return json.dumps(self.to_dict())

    def to_bytes(self) -> bytes:
        """
        Serialize request to bytes with null terminator (ImHex protocol).

        Returns:
            UTF-8 encoded JSON with \x00 terminator
        """
        return self.to_json().encode('utf-8') + b'\x00'


class JsonRpcResponse:
    """
    Represents a JSON-RPC 2.0 response.

    Examples:
        >>> response_json = '{"jsonrpc": "2.0", "id": 1, "result": {"key": "value"}}'
        >>> response = JsonRpcResponse.from_json(response_json)
        >>> response.is_success()
        True
        >>> response.get_result()
        {'key': 'value'}

        >>> error_json = '{"jsonrpc": "2.0", "id": 1, "error": {"code": -32601, "message": "Method not found"}}'
        >>> response = JsonRpcResponse.from_json(error_json)
        >>> response.is_error()
        True
        >>> response.get_error().code
        -32601
    """

    def __init__(
        self,
        response_id: Optional[Union[int, str]],
        result: Optional[Any] = None,
        error: Optional[JsonRpcError] = None
    ):
        """
        Initialize a JSON-RPC 2.0 response.

        Args:
            response_id: Response ID matching the request
            result: Result data (mutually exclusive with error)
            error: Error object (mutually exclusive with result)
        """
        self.response_id = response_id
        self.result = result
        self.error = error

    @classmethod
    def from_json(cls, json_str: str) -> 'JsonRpcResponse':
        """
        Parse JSON-RPC 2.0 response from JSON string.

        Args:
            json_str: JSON string to parse

        Returns:
            JsonRpcResponse object

        Raises:
            json.JSONDecodeError: If JSON is invalid
            ValueError: If response structure is invalid
        """
        try:
            data = json.loads(json_str)
        except json.JSONDecodeError as e:
            raise json.JSONDecodeError(f"Invalid JSON response: {e}", e.doc, e.pos)

        return cls.from_dict(data)

    @classmethod
    def from_dict(cls, data: Dict[str, Any]) -> 'JsonRpcResponse':
        """
        Parse JSON-RPC 2.0 response from dictionary.

        Args:
            data: Dictionary containing response data

        Returns:
            JsonRpcResponse object

        Raises:
            ValueError: If response structure is invalid
        """
        # Validate JSON-RPC version
        if data.get("jsonrpc") != "2.0":
            raise ValueError(f"Invalid JSON-RPC version: {data.get('jsonrpc')}")

        response_id = data.get("id")

        # Check for error
        if "error" in data:
            error_data = data["error"]
            error = JsonRpcError(
                code=error_data["code"],
                message=error_data["message"],
                data=error_data.get("data")
            )
            return cls(response_id=response_id, error=error)

        # Check for result
        if "result" in data:
            return cls(response_id=response_id, result=data["result"])

        # Response must have either result or error
        raise ValueError("Response must contain either 'result' or 'error'")

    def is_success(self) -> bool:
        """Check if response is successful (has result, no error)"""
        return self.error is None and self.result is not None

    def is_error(self) -> bool:
        """Check if response contains an error"""
        return self.error is not None

    def get_result(self) -> Any:
        """
        Get the result from successful response.

        Returns:
            Result data

        Raises:
            ValueError: If response contains an error
        """
        if self.is_error():
            raise ValueError(f"Response contains error: {self.error}")
        return self.result

    def get_error(self) -> JsonRpcError:
        """
        Get the error from failed response.

        Returns:
            JsonRpcError object

        Raises:
            ValueError: If response is successful
        """
        if not self.is_error():
            raise ValueError("Response is successful, no error present")
        return self.error  # type: ignore

    def __str__(self) -> str:
        if self.is_error():
            return f"JsonRpcResponse(id={self.response_id}, error={self.error})"
        return f"JsonRpcResponse(id={self.response_id}, result={self.result})"


def receive_response_bytes(sock) -> bytes:
    """
    Receive response bytes from socket until null terminator.

    This is a helper function for ImHex MCP protocol which terminates
    messages with a null byte (0x00).

    Args:
        sock: Socket object to receive from

    Returns:
        Received bytes without null terminator

    Raises:
        ConnectionError: If connection is closed
        TimeoutError: If socket times out
    """
    response_bytes = b''

    while True:
        chunk = sock.recv(1024)

        if not chunk:
            raise ConnectionError("Connection closed by server")

        response_bytes += chunk

        # Check for null terminator
        if b'\x00' in response_bytes:
            # Remove null terminator and any trailing bytes
            response_bytes = response_bytes.rstrip(b'\x00')
            break

    return response_bytes
