cmake_minimum_required(VERSION 3.20)
project(RagnarokClientReconstruction LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/W4 /permissive- /Zc:preprocessor /utf-8 /EHsc)
    add_compile_definitions(NOMINMAX WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

include_directories(include)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/Full Client")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/Full Client")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/Full Client")

# Find zlib for in-memory GRF decompression
find_package(ZLIB QUIET)

set(RO_SOURCES
    src/main.cpp
    src/core/exception_handler.cpp
    src/core/command_line.cpp
    src/core/application.cpp
    src/mode/mode_mgr.cpp
    src/mode/login_mode.cpp
    src/mode/game_mode.cpp
    src/resource/grf.cpp
    src/resource/inflate.cpp
    src/resource/resource_mgr.cpp
    src/resource/spr.cpp
    src/resource/act.cpp
    src/world/gat.cpp
    src/world/gnd.cpp
    src/world/rsw.cpp
    src/world/rsm.cpp
    src/resource/bmp.cpp
    src/actor/actor.cpp
    src/camera/camera.cpp
    src/render/renderer.cpp
)

add_executable(Ragexe WIN32 ${RO_SOURCES})

target_link_libraries(Ragexe PRIVATE
    winmm
    imm32
    ole32
    shell32
    gdi32
    dbghelp
)

if(ZLIB_FOUND)
    target_link_libraries(Ragexe PRIVATE ZLIB::ZLIB)
    target_compile_definitions(Ragexe PRIVATE HAVE_ZLIB)
endif()
