cmake_minimum_required(VERSION 3.16)

project(logiq_agent LANGUAGES CXX)

# ---------------------------------------------------------
# C++ Standard
# ---------------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile_commands.json for clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---------------------------------------------------------
# Source files
# ---------------------------------------------------------
add_executable(logiq-agent
    # Core
    src/main.cpp
    src/core/Agent.cpp

    # File handling
    src/file/FileFollower.cpp

    # Framing
    src/framing/LineFramer.cpp

    # Config
    src/config/ConfigLoader.cpp

    # Sinks
    src/sinks/HttpNdjsonSink.cpp

    # Router
    src/router/Router.cpp

    # Utils
    src/utils/Logger.cpp
)

# ---------------------------------------------------------
# Include directories
# ---------------------------------------------------------
target_include_directories(logiq-agent PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

# ---------------------------------------------------------
# Compiler warnings (recommended)
# ---------------------------------------------------------
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(logiq-agent PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Wconversion
    )
endif()
