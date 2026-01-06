# List Languages Tool

Get information about all supported programming languages and their compilation details.

## Tool Name
`list_languages`

## Description
Returns a list of all programming languages supported by the code execution server, along with compiler information, standards supported, and default resource limits.

## Parameters
None

## Return Value

Returns a JSON object with language information:

```json
{
  "supported_languages": [
    {
      "language": "c",
      "compiler": "riscv64-linux-gnu-gcc",
      "standard": "c17",
      "description": "C language with glibc support"
    },
    {
      "language": "cpp",
      "compiler": "riscv64-linux-gnu-g++",
      "standard": "c++20",
      "description": "C++ with full standard library and threading support"
    },
    {
      "language": "python",
      "interpreter": "python3",
      "version": "3.x",
      "description": "Python code (compiled to C extension)"
    },
    {
      "language": "rust",
      "compiler": "rustc",
      "target": "riscv64gc-unknown-linux-gnu",
      "description": "Rust with full standard library"
    }
  ],
  "default_limits": {
    "max_instructions": 36000000,
    "max_memory_bytes": 33554432,
    "timeout_seconds": 5
  }
}
```

## Example Usage

```json
{}
```

## Use Cases
- Check which languages are available before submitting code
- Get compiler versions and standards
- Understand default resource limits
- Build language-specific code generation logic
