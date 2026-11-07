cmake_minimum_required(VERSION 3.25)

project(MELEDiscordRPC LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 17)

if (NOT WIN32 OR NOT MSVC)
    message(FATAL_ERROR "This project requires MSVC on Windows x64")
endif()

set(CMAKE_POLICY_DEFAULT_CMP0091 NEW)

add_definitions(-DWIN32_LEAN_AND_MEAN -DNOMINMAX -DUNICODE -D_UNICODE)

add_library(MELEDiscordRPC SHARED
    src/Main.cpp
    src/Common.cpp
    src/Log.cpp
    src/DiscordIpc.cpp
    src/Ini.cpp
)
target_include_directories(MELEDiscordRPC PRIVATE src)
target_link_libraries(MELEDiscordRPC PRIVATE shlwapi)
set_property(TARGET MELEDiscordRPC PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set_property(TARGET MELEDiscordRPC PROPERTY OUTPUT_NAME "MELEPerGamePresence")
set_property(TARGET MELEDiscordRPC PROPERTY SUFFIX ".asi")
set_property(TARGET MELEDiscordRPC PROPERTY RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/out")
set_property(TARGET MELEDiscordRPC PROPERTY RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/out")
set_property(TARGET MELEDiscordRPC PROPERTY RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/out")

target_compile_options(MELEDiscordRPC PRIVATE /utf-8 /W3 /EHsc)

add_executable(rpc_test
    toolkit/rpc_test/rpc_test.cpp
    src/Common.cpp
    src/Log.cpp
    src/DiscordIpc.cpp
)
target_include_directories(rpc_test PRIVATE src)
target_link_libraries(rpc_test PRIVATE shlwapi)
set_property(TARGET rpc_test PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set_property(TARGET rpc_test PROPERTY RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/out")
set_property(TARGET rpc_test PROPERTY RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/out")
set_property(TARGET rpc_test PROPERTY RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/out")
target_compile_options(rpc_test PRIVATE /utf-8 /W3 /EHsc)

add_executable(rpc_probe
    toolkit/rpc_probe/rpc_probe.cpp
    src/Common.cpp
    src/Log.cpp
    src/DiscordIpc.cpp
)
target_include_directories(rpc_probe PRIVATE src)
target_link_libraries(rpc_probe PRIVATE shlwapi)
set_property(TARGET rpc_probe PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set_property(TARGET rpc_probe PROPERTY RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/out")
set_property(TARGET rpc_probe PROPERTY RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/out")
set_property(TARGET rpc_probe PROPERTY RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/out")
target_compile_options(rpc_probe PRIVATE /utf-8 /W3 /EHsc)
