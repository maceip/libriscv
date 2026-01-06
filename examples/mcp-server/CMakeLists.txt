cmake_minimum_required(VERSION 3.14)
project(mcp-server CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(LTO "Enable interprocedural optimizations" ON)
option(NATIVE "Enable native instructions" ON)
option(USE_QUICKJS "Enable QuickJS for full JavaScript runtime support" ON)

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

# QuickJS support
if(USE_QUICKJS)
    set(QUICKJS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/quickjs-riscv")
    set(QUICKJS_INCLUDE "${QUICKJS_DIR}/include")
    set(QUICKJS_LIB "${QUICKJS_DIR}/lib/libquickjs.a")
    set(QUICKJS_BINARY "${QUICKJS_DIR}/bin/qjs")

    # Check if QuickJS is built
    if(NOT EXISTS "${QUICKJS_LIB}")
        message(STATUS "QuickJS not found, building for RISC-V...")
        execute_process(
            COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/third_party/build_quickjs.sh
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/third_party
            RESULT_VARIABLE QUICKJS_BUILD_RESULT
        )
        if(NOT QUICKJS_BUILD_RESULT EQUAL 0)
            message(WARNING "Failed to build QuickJS, disabling support")
            set(USE_QUICKJS OFF)
        else()
            message(STATUS "QuickJS built successfully")
        endif()
    else()
        message(STATUS "Found QuickJS: ${QUICKJS_LIB}")
    endif()

    if(USE_QUICKJS)
        add_definitions(-DUSE_QUICKJS=1)
    endif()
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

# Add QuickJS include directory if enabled
if(USE_QUICKJS)
    target_include_directories(mcp-server PRIVATE ${QUICKJS_INCLUDE})
endif()

# Link libraries
target_link_libraries(mcp-server PRIVATE
    riscv
    nlohmann_json::nlohmann_json
    pthread
    m
    stdc++fs  # For std::filesystem on older compilers
)

# Link QuickJS library if enabled
if(USE_QUICKJS)
    target_link_libraries(mcp-server PRIVATE ${QUICKJS_LIB} dl)
endif()

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
