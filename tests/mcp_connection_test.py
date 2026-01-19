#!/usr/bin/env python3
"""
ImHex MCP Connection Test
Tests connectivity and protocol validation with ImHex MCP server.

Usage:
    python tests/mcp_connection_test.py

Prerequisites:
    - ImHex running with MCP Server enabled (Settings > General > MCP Server)
    - Python 3.8+
"""

import socket
import json
import sys
from typing import Optional, Dict, Any


class Colors:
    """ANSI color codes for terminal output"""
    GREEN = '\033[92m'
    RED = '\033[91m'
    YELLOW = '\033[93m'
    BLUE = '\033[94m'
    RESET = '\033[0m'
    BOLD = '\033[1m'


def print_success(message: str):
    """Print success message in green"""
    print(f"{Colors.GREEN}✓{Colors.RESET} {message}")


def print_error(message: str):
    """Print error message in red"""
    print(f"{Colors.RED}✗{Colors.RESET} {message}")


def print_info(message: str):
    """Print info message in blue"""
    print(f"{Colors.BLUE}ℹ{Colors.RESET} {message}")


def print_warning(message: str):
    """Print warning message in yellow"""
    print(f"{Colors.YELLOW}⚠{Colors.RESET} {message}")


class ImHexMCPTester:
    """Tests connection to ImHex MCP server"""

    def __init__(self, host: str = '127.0.0.1', port: int = 19743):
        self.host = host
        self.port = port
        self.sock: Optional[socket.socket] = None
        self.request_id = 0

    def connect(self) -> bool:
        """Establish TCP connection to MCP server"""
        try:
            self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.sock.settimeout(5.0)
            self.sock.connect((self.host, self.port))
            print_success(f"Conectado a {self.host}:{self.port}")
            return True
        except ConnectionRefusedError:
            print_error(f"Conexão recusada em {self.host}:{self.port}")
            print_warning("Certifique-se de que:")
            print("  1. ImHex está rodando")
            print("  2. MCP Server está habilitado em Settings > General > MCP Server")
            return False
        except socket.timeout:
            print_error("Timeout ao conectar")
            return False
        except Exception as e:
            print_error(f"Erro ao conectar: {e}")
            return False

    def send_request(self, method: str, params: Optional[Dict[str, Any]] = None) -> Optional[Dict[str, Any]]:
        """Send JSON-RPC 2.0 request and receive response"""
        if not self.sock:
            print_error("Socket não conectado")
            return None

        self.request_id += 1
        request = {
            "jsonrpc": "2.0",
            "id": self.request_id,
            "method": method
        }

        if params is not None:
            request["params"] = params

        try:
            # Serialize and send request with null terminator
            request_json = json.dumps(request)
            request_bytes = request_json.encode('utf-8') + b'\x00'
            self.sock.sendall(request_bytes)

            print_info(f"Enviado: {method}")

            # Receive response (read until null terminator)
            response_bytes = b''
            while True:
                chunk = self.sock.recv(1024)
                if not chunk:
                    print_error("Conexão fechada pelo servidor")
                    return None

                response_bytes += chunk

                # Check for null terminator
                if b'\x00' in response_bytes:
                    response_bytes = response_bytes.rstrip(b'\x00')
                    break

            # Parse response
            response_json = response_bytes.decode('utf-8')
            response = json.loads(response_json)

            # Check for errors
            if "error" in response:
                error = response["error"]
                print_error(f"Erro JSON-RPC: {error.get('message', 'Unknown error')} (code: {error.get('code')})")
                return None

            return response.get("result")

        except json.JSONDecodeError as e:
            print_error(f"Erro ao decodificar JSON: {e}")
            return None
        except Exception as e:
            print_error(f"Erro ao enviar requisição: {e}")
            return None

    def test_initialize(self) -> bool:
        """Test initialize handshake"""
        print("\n" + Colors.BOLD + "Teste 1: Initialize Handshake" + Colors.RESET)

        params = {
            "protocolVersion": "2025-06-18",
            "clientInfo": {
                "name": "ImHex MCP Connection Tester",
                "version": "1.0.0"
            }
        }

        result = self.send_request("initialize", params)

        if not result:
            print_error("Initialize falhou")
            return False

        # Validate response
        if "protocolVersion" not in result:
            print_error("Resposta não contém protocolVersion")
            return False

        protocol_version = result["protocolVersion"]
        server_info = result.get("serverInfo", {})

        print_success(f"Initialize aceito (protocol {protocol_version})")
        print_info(f"Servidor: {server_info.get('name', 'Unknown')} v{server_info.get('version', 'Unknown')}")

        return True

    def test_initialized_notification(self) -> bool:
        """Send initialized notification"""
        print("\n" + Colors.BOLD + "Teste 2: Initialized Notification" + Colors.RESET)

        # Notifications don't have responses, but we send them anyway
        try:
            notification = {
                "jsonrpc": "2.0",
                "method": "notifications/initialized"
            }

            notification_json = json.dumps(notification)
            notification_bytes = notification_json.encode('utf-8') + b'\x00'
            self.sock.sendall(notification_bytes)

            print_success("Notification 'initialized' enviada")
            return True

        except Exception as e:
            print_error(f"Erro ao enviar notification: {e}")
            return False

    def test_list_tools(self) -> bool:
        """Test tools/list to enumerate available tools"""
        print("\n" + Colors.BOLD + "Teste 3: List Tools" + Colors.RESET)

        result = self.send_request("tools/list")

        if not result:
            print_error("tools/list falhou")
            return False

        if "tools" not in result:
            print_error("Resposta não contém 'tools'")
            return False

        tools = result["tools"]

        if not isinstance(tools, list):
            print_error("'tools' não é uma lista")
            return False

        print_success(f"{len(tools)} ferramentas disponíveis:")

        for tool in tools:
            name = tool.get("name", "Unknown")
            description = tool.get("description", "No description")
            print(f"  • {Colors.BOLD}{name}{Colors.RESET}: {description[:60]}...")

        return True

    def disconnect(self):
        """Close connection"""
        if self.sock:
            try:
                self.sock.close()
                print_info("Conexão fechada")
            except Exception as e:
                print_warning(f"Erro ao fechar conexão: {e}")

    def run_tests(self) -> bool:
        """Run all tests"""
        print(Colors.BOLD + "=" * 60 + Colors.RESET)
        print(Colors.BOLD + "ImHex MCP Connection Test Suite" + Colors.RESET)
        print(Colors.BOLD + "=" * 60 + Colors.RESET)

        # Connect
        if not self.connect():
            return False

        try:
            # Test initialize
            if not self.test_initialize():
                return False

            # Test initialized notification
            if not self.test_initialized_notification():
                return False

            # Test list tools
            if not self.test_list_tools():
                return False

            print("\n" + Colors.BOLD + "=" * 60 + Colors.RESET)
            print_success("Todos os testes passaram!")
            print(Colors.BOLD + "=" * 60 + Colors.RESET)

            return True

        finally:
            self.disconnect()


def main():
    """Main entry point"""
    tester = ImHexMCPTester()
    success = tester.run_tests()

    sys.exit(0 if success else 1)


if __name__ == "__main__":
    main()
