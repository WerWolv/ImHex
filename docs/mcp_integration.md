# ImHex MCP (Machine Command Protocol) Integration

This document describes the MCP integration feature in ImHex, allowing external tools or scripts to interact with the hex editor programmatically. The integration is facilitated by the "MCP Integration" plugin.

## Overview

The MCP integration uses a JSON-RPC like protocol for sending commands to ImHex and receiving responses. The "MCP Integration" plugin must be enabled in ImHex for this functionality to be available.

Currently, commands are processed via an internal C++ function: `hex::plugins::mcp::process_mcp_command(const std::string& command_json)`. Future developments may include IPC mechanisms like sockets or named pipes for easier external access.

## Command Protocol

All communication is done using JSON objects.

### Request Object

A request to execute a command must be a JSON object with the following members:

- `jsonrpc`: (String) Must be exactly "2.0".
- `method`: (String) The name of the command to be executed.
- `params`: (Object|Array) A structured value holding the parameters for the command. Can be omitted if the command requires no parameters.
- `id`: (String|Number|Null) An identifier established by the client. If not Null, the server must reply with the same id in its response.

Example Request:
```json
{
  "jsonrpc": "2.0",
  "method": "read_bytes",
  "params": {"offset": 256, "count": 16},
  "id": "req-123"
}
```

### Response Object

The response from ImHex will also be a JSON object with the following members:

- `jsonrpc`: (String) Will be "2.0".
- `result`: (Any) This member is **required** on success. The value depends on the command executed.
- `error`: (Object) This member is **required** on failure. It must be an object containing:
    - `code`: (Number) A number that indicates the error type that occurred.
    - `message`: (String) A short description of the error.
    - `data`: (Any) Optional. A primitive or structured value containing additional information about the error.
- `id`: (String|Number|Null) The id from the request it is responding to.

Example Successful Response:
```json
{
  "jsonrpc": "2.0",
  "result": {
    "offset": 256,
    "count": 16,
    "hex_data": "000102030405060708090a0b0c0d0e0f"
  },
  "id": "req-123"
}
```

Example Error Response:
```json
{
  "jsonrpc": "2.0",
  "error": {
    "code": -32601,
    "message": "Method not found: unknown_method"
  },
  "id": "req-error"
}
```

## Common Error Codes

The MCP integration uses standard JSON-RPC 2.0 error codes. Common ones include:

- `-32700`: Parse error. Invalid JSON was received by the server.
- `-32600`: Invalid Request. The JSON sent is not a valid Request object (e.g., wrong `jsonrpc` version, missing `method`).
- `-32601`: Method not found. The method does not exist / is not available.
- `-32602`: Invalid params. Invalid method parameter(s) (e.g., missing required parameter, wrong type).
- `-32603`: Internal error. Internal JSON-RPC error on the server side.

Command-specific errors (e.g., "No active data provider", "Provider is not writable") are typically returned with one of these codes (often -32602 for bad parameters leading to an invalid state, or a general server error code like -32000 to -32099 if not parameter related). The `message` field will contain specific details.

## Available Commands

Below is a list of currently supported commands.

---

### 1. `get_selection`

Retrieves the currently selected region in the hex editor.

- **Parameters:** None
- **Request:**
  ```json
  {
    "jsonrpc": "2.0",
    "method": "get_selection",
    "id": "sel-1"
  }
  ```
- **Result:** An object containing:
    - `start_offset`: (Number|Null) The starting offset of the selection. Null if no selection.
    - `size`: (Number) The size of the selection in bytes. 0 if no selection.
    - `end_offset`: (Number|Null) The ending offset of the selection (inclusive). Null if no selection.
- **Successful Response (with selection):**
  ```json
  {
    "jsonrpc": "2.0",
    "result": {
      "start_offset": 1024,
      "size": 16,
      "end_offset": 1039
    },
    "id": "sel-1"
  }
  ```
- **Successful Response (no selection):**
  ```json
  {
    "jsonrpc": "2.0",
    "result": {
      "start_offset": null,
      "size": 0,
      "end_offset": null
    },
    "id": "sel-no-selection"
  }
  ```

---

### 2. `set_selection`

Sets the current selection in the hex editor.

- **Parameters:**
    - `start_offset`: (Number, Required) The starting offset for the selection.
    - `size`: (Number, Required) The size of the selection in bytes.
- **Request:**
  ```json
  {
    "jsonrpc": "2.0",
    "method": "set_selection",
    "params": {"start_offset": 512, "size": 32},
    "id": "setsel-1"
  }
  ```
- **Result:** An object confirming the action:
    - `status`: (String) "success".
    - `start_offset`: (Number) The applied start offset.
    - `size`: (Number) The applied size.
- **Successful Response:**
  ```json
  {
    "jsonrpc": "2.0",
    "result": {
      "status": "success",
      "start_offset": 512,
      "size": 32
    },
    "id": "setsel-1"
  }
  ```
- **Error (e.g., invalid parameters):**
  ```json
  {
    "jsonrpc": "2.0",
    "error": {
        "code": -32602,
        "message": "Missing or invalid 'start_offset' parameter"
    },
    "id": "setsel-error"
  }
  ```

---

### 3. `read_bytes`

Reads a sequence of bytes from a specified offset.

- **Parameters:**
    - `offset`: (Number, Required) The starting offset to read from.
    - `count`: (Number, Required) The number of bytes to read.
- **Request:**
  ```json
  {
    "jsonrpc": "2.0",
    "method": "read_bytes",
    "params": {"offset": 1024, "count": 4},
    "id": "read-1"
  }
  ```
- **Result:** An object containing:
    - `offset`: (Number) The offset from which data was read.
    - `count`: (Number) The original requested number of bytes.
    - `bytes_read`: (Number) The actual number of bytes read (can be less than count if EOF is reached).
    - `hex_data`: (String) The data read, represented as a hexadecimal string.
- **Successful Response:**
  ```json
  {
    "jsonrpc": "2.0",
    "result": {
      "offset": 1024,
      "count": 4,
      "bytes_read": 4,
      "hex_data": "deadbeef"
    },
    "id": "read-1"
  }
  ```
- **Error (e.g., read out of bounds, no provider):**
  ```json
  {
    "jsonrpc": "2.0",
    "error": {
        "code": -32001,
        "message": "No active data provider"
    },
    "id": "read-error"
  }
  ```

---

### 4. `write_bytes`

Writes a sequence of bytes to a specified offset.

- **Parameters:**
    - `offset`: (Number, Required) The starting offset to write to.
    - `data`: (String, Required) The data to write, represented as a hexadecimal string.
- **Request:**
  ```json
  {
    "jsonrpc": "2.0",
    "method": "write_bytes",
    "params": {"offset": 2048, "data": "cafebabe"},
    "id": "write-1"
  }
  ```
- **Result:** An object confirming the write operation:
    - `offset`: (Number) The offset where data was written.
    - `bytes_written`: (Number) The number of bytes written.
    - `status`: (String) "success".
- **Successful Response:**
  ```json
  {
    "jsonrpc": "2.0",
    "result": {
      "offset": 2048,
      "bytes_written": 4,
      "status": "success"
    },
    "id": "write-1"
  }
  ```
- **Error (e.g., provider not writable, invalid hex data):**
  ```json
  {
    "jsonrpc": "2.0",
    "error": {
        "code": -32003,
        "message": "Provider is not writable"
    },
    "id": "write-error"
  }
  ```

---

### 5. `search`

Searches for a hex pattern within a specified range or the entire data.

- **Parameters:**
    - `pattern`: (String, Required) The hexadecimal string pattern to search for.
    - `start_offset`: (Number, Optional) The offset to start searching from. Defaults to `0`.
    - `end_offset`: (Number, Optional) The offset to search up to (exclusive for search range, but pattern can match across this boundary). Defaults to the end of the data.
- **Request:**
  ```json
  {
    "jsonrpc": "2.0",
    "method": "search",
    "params": {"pattern": "4A6F686E", "start_offset": 0},
    "id": "search-1"
  }
  ```
- **Result:** An object containing:
    - `offsets`: (Array of Numbers) A list of starting offsets where the pattern was found.
- **Successful Response:**
  ```json
  {
    "jsonrpc": "2.0",
    "result": {
      "offsets": [100, 2506, 30120]
    },
    "id": "search-1"
  }
  ```
- **Error (e.g., no provider, invalid pattern):**
  ```json
  {
    "jsonrpc": "2.0",
    "error": {
        "code": -32602,
        "message": "Pattern cannot be empty"
    },
    "id": "search-error"
  }
  ```

---

### 6. `get_offset_info`

Retrieves basic information about a specific offset.

- **Parameters:**
    - `offset`: (Number, Required) The offset to get information about.
- **Request:**
  ```json
  {
    "jsonrpc": "2.0",
    "method": "get_offset_info",
    "params": {"offset": 4096},
    "id": "info-1"
  }
  ```
- **Result:** An object containing:
    - `offset`: (Number) The queried offset.
    - `address_str`: (String) The offset formatted as a string (e.g., "0x00001000").
    (Future versions may include more details like segment info, data interpretations, etc.)
- **Successful Response:**
  ```json
  {
    "jsonrpc": "2.0",
    "result": {
      "offset": 4096,
      "address_str": "0x00001000"
    },
    "id": "info-1"
  }
  ```
- **Error (e.g., invalid offset):**
  ```json
  {
    "jsonrpc": "2.0",
    "error": {
        "code": -32602,
        "message": "Missing or invalid 'offset' parameter"
    },
    "id": "info-error"
  }
  ```

## How to Use

1.  Ensure the "MCP Integration" plugin is enabled in ImHex.
2.  (Currently) If you are developing another ImHex plugin or have access to its C++ internals, you can call:
    `std::string response = hex::plugins::mcp::process_mcp_command("your_json_command_string");`
3.  Parse the JSON `response` string to get the result or error.

Future enhancements aim to provide more direct external access (e.g., via a local network socket).
```
