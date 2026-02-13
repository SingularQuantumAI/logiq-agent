
cmake_minimum_required(VERSION 3.16)
project(logiq_agent LANGUAGES CXX)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(logiq-agent
    src/main.cpp
    src/core/Agent.cpp
    src/config/ConfigLoader.cpp
    src/utils/Logger.cpp
    src/router/Router.cpp
    src/sinks/HttpNdjsonSink.cpp
)

target_include_directories(logiq-agent PRIVATE src)
