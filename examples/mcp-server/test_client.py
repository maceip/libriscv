#!/usr/bin/env python3
"""
Simple test client for the libriscv MCP Code Execution Server

This script sends test requests to the MCP server and displays responses.
"""

import json
import subprocess
import sys
from typing import Any, Dict

def send_request(proc: subprocess.Popen, request: Dict[str, Any]) -> Dict[str, Any]:
    """Send a JSON-RPC request and get the response."""
    request_str = json.dumps(request) + "\n"
    proc.stdin.write(request_str.encode())
    proc.stdin.flush()

    response_line = proc.stdout.readline().decode().strip()
    if not response_line:
        raise Exception("Server closed connection")

    return json.loads(response_line)

def print_separator():
    print("\n" + "="*80 + "\n")

def test_initialize(proc):
    """Test the initialize method."""
    print("TEST 1: Initialize")
    print("-" * 80)

    request = {
        "jsonrpc": "2.0",
        "id": 1,
        "method": "initialize",
        "params": {
            "protocolVersion": "2024-11-05",
            "capabilities": {},
            "clientInfo": {
                "name": "test-client",
                "version": "1.0.0"
            }
        }
    }

    print("Request:")
    print(json.dumps(request, indent=2))

    response = send_request(proc, request)

    print("\nResponse:")
    print(json.dumps(response, indent=2))

    assert response.get("jsonrpc") == "2.0"
    assert "result" in response
    print("\n✓ Initialize test passed")

def test_list_tools(proc):
    """Test listing available tools."""
    print("TEST 2: List Tools")
    print("-" * 80)

    request = {
        "jsonrpc": "2.0",
        "id": 2,
        "method": "tools/list"
    }

    print("Request:")
    print(json.dumps(request, indent=2))

    response = send_request(proc, request)

    print("\nResponse:")
    print(json.dumps(response, indent=2))

    assert "result" in response
    assert "tools" in response["result"]
    print(f"\n✓ Found {len(response['result']['tools'])} tools")

def test_list_languages(proc):
    """Test the list_languages tool."""
    print("TEST 3: List Languages")
    print("-" * 80)

    request = {
        "jsonrpc": "2.0",
        "id": 3,
        "method": "tools/call",
        "params": {
            "name": "list_languages",
            "arguments": {}
        }
    }

    print("Request:")
    print(json.dumps(request, indent=2))

    response = send_request(proc, request)

    print("\nResponse:")
    print(json.dumps(response, indent=2))

    assert "result" in response
    print("\n✓ List languages test passed")

def test_execute_cpp_hello(proc):
    """Test executing a simple C++ Hello World program."""
    print("TEST 4: Execute C++ Hello World")
    print("-" * 80)

    code = """#include <iostream>

int main() {
    std::cout << "Hello from libriscv MCP!" << std::endl;
    return 42;
}
"""

    request = {
        "jsonrpc": "2.0",
        "id": 4,
        "method": "tools/call",
        "params": {
            "name": "execute_code",
            "arguments": {
                "code": code,
                "language": "cpp",
                "timeout_seconds": 5,
                "max_memory_mb": 32
            }
        }
    }

    print("Request:")
    print(json.dumps(request, indent=2))

    response = send_request(proc, request)

    print("\nResponse:")
    print(json.dumps(response, indent=2))

    # Parse the result
    if "result" in response and "content" in response["result"]:
        content = response["result"]["content"][0]["text"]
        result = json.loads(content)
        print("\nExecution Result:")
        print(f"  Success: {result.get('success')}")
        print(f"  Exit Code: {result.get('exit_code')}")
        print(f"  Output: {result.get('output')}")
        print(f"  Execution Time: {result.get('execution_time_us')} μs")
        print(f"  Instructions: {result.get('instructions_executed')}")
        print(f"  Memory Used: {result.get('memory_used')} bytes")

        assert result.get('success') == True
        assert result.get('exit_code') == 42
        assert "Hello from libriscv MCP!" in result.get('output', '')

    print("\n✓ C++ execution test passed")

def test_execute_c_fibonacci(proc):
    """Test executing a C Fibonacci program."""
    print("TEST 5: Execute C Fibonacci")
    print("-" * 80)

    code = """#include <stdio.h>

int fib(int n) {
    if (n <= 1) return n;
    return fib(n-1) + fib(n-2);
}

int main() {
    int result = fib(10);
    printf("fib(10) = %d\\n", result);
    return 0;
}
"""

    request = {
        "jsonrpc": "2.0",
        "id": 5,
        "method": "tools/call",
        "params": {
            "name": "execute_code",
            "arguments": {
                "code": code,
                "language": "c"
            }
        }
    }

    print("Request:")
    print(json.dumps(request, indent=2))

    response = send_request(proc, request)

    print("\nResponse:")
    print(json.dumps(response, indent=2))

    # Parse the result
    if "result" in response and "content" in response["result"]:
        content = response["result"]["content"][0]["text"]
        result = json.loads(content)
        print("\nExecution Result:")
        print(f"  Success: {result.get('success')}")
        print(f"  Output: {result.get('output')}")

        assert result.get('success') == True
        assert "fib(10) = 55" in result.get('output', '')

    print("\n✓ C Fibonacci test passed")

def test_sanitization_failure(proc):
    """Test that dangerous code is blocked."""
    print("TEST 6: Code Sanitization (should fail)")
    print("-" * 80)

    code = """#include <stdlib.h>
#include <stdio.h>

int main() {
    system("echo 'This should be blocked'");
    return 0;
}
"""

    request = {
        "jsonrpc": "2.0",
        "id": 6,
        "method": "tools/call",
        "params": {
            "name": "execute_code",
            "arguments": {
                "code": code,
                "language": "c"
            }
        }
    }

    print("Request:")
    print(json.dumps(request, indent=2))

    response = send_request(proc, request)

    print("\nResponse:")
    print(json.dumps(response, indent=2))

    # Should return an error
    if "result" in response and "content" in response["result"]:
        content = response["result"]["content"][0]["text"]
        result = json.loads(content)
        print("\nExecution Result:")
        print(f"  Success: {result.get('success')}")
        if not result.get('success'):
            print(f"  Error: {result.get('error')}")
            print("\n✓ Sanitization correctly blocked dangerous code")
        else:
            print("\n✗ WARNING: Dangerous code was not blocked!")
    elif "error" in response:
        print("\n✓ Server correctly rejected dangerous code")

def main():
    """Run all tests."""
    print("\n" + "="*80)
    print("libriscv MCP Code Execution Server - Test Client")
    print("="*80)

    # Start the MCP server
    server_path = "./build/mcp-server"

    try:
        print(f"\nStarting server: {server_path}")
        proc = subprocess.Popen(
            [server_path],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE
        )

        print("✓ Server started")
        print_separator()

        # Run tests
        try:
            test_initialize(proc)
            print_separator()

            test_list_tools(proc)
            print_separator()

            test_list_languages(proc)
            print_separator()

            test_execute_cpp_hello(proc)
            print_separator()

            test_execute_c_fibonacci(proc)
            print_separator()

            test_sanitization_failure(proc)
            print_separator()

            print("\n" + "="*80)
            print("ALL TESTS PASSED! ✓")
            print("="*80 + "\n")

        except Exception as e:
            print(f"\n✗ Test failed: {e}")
            stderr = proc.stderr.read().decode()
            if stderr:
                print(f"\nServer stderr:\n{stderr}")
            return 1
        finally:
            proc.terminate()
            proc.wait()

    except FileNotFoundError:
        print(f"\n✗ Server executable not found: {server_path}")
        print("\nPlease build the server first:")
        print("  ./build.sh")
        return 1

    return 0

if __name__ == "__main__":
    sys.exit(main())
