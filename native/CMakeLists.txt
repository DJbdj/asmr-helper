cmake_minimum_required(VERSION 3.15)
project(asmr-helper VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 99)

# Source directory
set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)

# Common C++ sources
set(CPP_SRCS
    ${SRC_DIR}/main.cpp
    ${SRC_DIR}/ui.cpp
    ${SRC_DIR}/http.cpp
    ${SRC_DIR}/video.cpp
    ${SRC_DIR}/platform.cpp
)

# Platform-specific linenoise source
if(WIN32)
    set(LINENOISE_SRC ${SRC_DIR}/linenoise_win.c)
else()
    set(LINENOISE_SRC ${SRC_DIR}/linenoise.c)
endif()

# All sources
set(ALL_SRCS ${CPP_SRCS} ${LINENOISE_SRC})

# Add executable
add_executable(asmr-helper ${ALL_SRCS})

# Target include directories
target_include_directories(asmr-helper PRIVATE ${SRC_DIR})

# Compiler-specific options
if(MSVC)
    target_compile_options(asmr-helper PRIVATE /W3 /utf-8)
    target_compile_definitions(asmr-helper PRIVATE _CRT_SECURE_NO_WARNINGS _CRT_NONSTDC_NO_WARNINGS)
else()
    target_compile_options(asmr-helper PRIVATE -Wall -O2)
endif()

# Platform dependencies
if(WIN32)
    # Windows: WinHTTP (built-in, no external dependency)
    target_link_libraries(asmr-helper PRIVATE winhttp shlwapi ws2_32)
else()
    # Unix: CURL
    find_package(CURL REQUIRED)
    target_link_libraries(asmr-helper PRIVATE CURL::libcurl)
endif()

# Copy FFmpeg directory to build output (Windows convenience)
if(WIN32 AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/ffmpeg")
    add_custom_command(TARGET asmr-helper POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/ffmpeg
            $<TARGET_FILE_DIR:asmr-helper>/ffmpeg
        COMMENT "Copying FFmpeg to output directory"
    )
endif()
