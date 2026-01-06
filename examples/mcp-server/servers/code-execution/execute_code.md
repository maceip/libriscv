# Execute Code Tool

Execute code in a sandboxed RISC-V environment using libriscv.

## Tool Name
`execute_code`

## Description
Compiles and executes code in multiple programming languages within a secure sandboxed environment. The code is compiled to RISC-V architecture, sanitized for security, and executed with strict resource limits to prevent malicious behavior.

## Supported Languages
- **C**: Full C17 support with glibc
- **C++**: Full C++20 support with standard library and threading
- **Python**: Python code (compiled to C extension) - limited support
- **Rust**: Rust with standard library support

## Parameters

### `code` (required)
- **Type**: string
- **Description**: The source code to execute
- **Example**:
```cpp
#include <iostream>
int main() {
    std::cout << "Hello from libriscv!" << std::endl;
    return 0;
}
```

### `language` (optional)
- **Type**: string
- **Enum**: `c`, `cpp`, `python`, `rust`
- **Default**: `cpp`
- **Description**: Programming language of the provided code

### `timeout_seconds` (optional)
- **Type**: number
- **Default**: 5
- **Min**: 1
- **Max**: 30
- **Description**: Maximum execution time in seconds before the program is terminated

### `max_memory_mb` (optional)
- **Type**: number
- **Default**: 32
- **Min**: 1
- **Max**: 128
- **Description**: Maximum memory the program can allocate in megabytes

## Return Value

Returns a JSON object with execution results:

```json
{
  "success": true,
  "exit_code": 0,
  "output": "Hello from libriscv!\n",
  "execution_time_us": 1234,
  "compile_time_us": 5678,
  "instructions_executed": 10000,
  "memory_used": 8192,
  "binary_size": 12345,
  "compilation_output": "",
  "error": "",
  "exception": ""
}
```

### Fields

- **success**: `boolean` - Whether execution completed successfully
- **exit_code**: `number` - Program exit code (from return value or exit())
- **output**: `string` - Combined stdout/stderr from the program
- **execution_time_us**: `number` - Execution time in microseconds
- **compile_time_us**: `number` - Compilation time in microseconds
- **instructions_executed**: `number` - Number of RISC-V instructions executed
- **memory_used**: `number` - Memory used in bytes
- **binary_size**: `number` - Size of compiled binary in bytes
- **compilation_output**: `string` - Compiler output (warnings/errors)
- **error**: `string` - Error message if execution failed
- **exception**: `string` - Exception message if program threw an exception

## Security Features

### Code Sanitization
All code is sanitized before compilation to prevent:
- System calls to `system()`, `popen()`, `exec*()`
- Process forking with `fork()`
- Dynamic library loading with `dlopen()`/`dlsym()`
- Direct network socket operations
- Path traversal in `#include` directives
- Inline assembly (restricted)

### Resource Limits
- **Maximum binary size**: 32 MB
- **Maximum instructions**: 36 million (prevents infinite loops)
- **Memory limit**: Configurable up to 128 MB
- **Execution timeout**: Configurable up to 30 seconds

### Sandboxing
Code executes in a complete RISC-V sandbox:
- Isolated from host system
- No direct file system access
- No network access
- Limited syscall interface
- Memory access virtualized and monitored

## Example Usage

### C++ Example
```json
{
  "code": "#include <iostream>\n#include <vector>\n\nint main() {\n    std::vector<int> nums = {1, 2, 3, 4, 5};\n    int sum = 0;\n    for (int n : nums) sum += n;\n    std::cout << \"Sum: \" << sum << std::endl;\n    return 0;\n}",
  "language": "cpp",
  "timeout_seconds": 5,
  "max_memory_mb": 32
}
```

### C Example
```json
{
  "code": "#include <stdio.h>\n\nint main() {\n    printf(\"Hello from C!\\n\");\n    return 0;\n}",
  "language": "c"
}
```

### Rust Example
```json
{
  "code": "fn main() {\n    println!(\"Hello from Rust!\");\n}",
  "language": "rust",
  "max_memory_mb": 64
}
```

## Error Handling

The tool may return errors for:
- **Empty code**: Code parameter cannot be empty
- **Compilation failure**: Syntax errors, missing includes, etc.
- **Sanitization failure**: Code contains forbidden patterns
- **Resource limit exceeded**: Timeout, memory limit, or instruction limit exceeded
- **Sandbox error**: Error in the execution environment

Check the `success` field and `error`/`exception` fields for details.

## Notes

- Compilation happens on-demand for each execution
- Temporary files are created in `/tmp/mcp-code-exec/` and cleaned up after execution
- The sandbox provides a minimal Linux environment (libc, pthread, etc.)
- Standard library functions are fully supported within resource limits
