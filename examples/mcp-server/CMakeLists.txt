cmake_minimum_required(VERSION 3.14)
project(mcp-server CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(LTO "Enable interprocedural optimizations" ON)
option(NATIVE "Enable native instructions" ON)

# Add libriscv library
add_subdirectory(../../lib libriscv)

# Find or fetch nlohmann/json
find_package(nlohmann_json 3.2.0 QUIET)
if(NOT nlohmann_json_FOUND)
    message(STATUS "nlohmann_json not found, fetching from GitHub...")
    include(FetchContent)
    FetchContent_Declare(json
        URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_MakeAvailable(json)
endif()

# Source files
set(SOURCES
    src/mcp_server.cpp
    src/code_executor.cpp
    src/sanitizer.cpp
)

# Create executable
add_executable(mcp-server ${SOURCES})

# Include directories
target_include_directories(mcp-server PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_BINARY_DIR}
)

# Link libraries
target_link_libraries(mcp-server PRIVATE
    riscv
    nlohmann_json::nlohmann_json
    pthread
    m
    stdc++fs  # For std::filesystem on older compilers
)

# Compiler options
target_compile_options(mcp-server PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)

# LTO support
if(LTO)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT supported OUTPUT error)
    if(supported)
        message(STATUS "IPO / LTO enabled")
        set_property(TARGET mcp-server PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
    else()
        message(STATUS "IPO / LTO not supported: <${error}>")
    endif()
endif()

# Native optimizations
if(NATIVE)
    target_compile_options(riscv PUBLIC "-march=native")
endif()

# Optimize libriscv
target_compile_options(riscv PUBLIC -O3)

# Install target
install(TARGETS mcp-server DESTINATION bin)

# Install server tool definitions
install(DIRECTORY servers/ DESTINATION share/mcp-server/servers)
