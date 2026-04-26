cmake_minimum_required(VERSION 3.15)
project(asmr-helper VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 99)

# FFmpeg static linking option
set(FFMPEG_STATIC OFF CACHE BOOL "Statically link FFmpeg libraries (no external FFmpeg binary needed)")

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

# FFmpeg engine source (only when FFMPEG_STATIC is ON)
if(FFMPEG_STATIC)
    list(APPEND CPP_SRCS ${SRC_DIR}/ffmpeg_engine.cpp)
endif()

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

# FFmpeg static linking
if(FFMPEG_STATIC)
    target_compile_definitions(asmr-helper PRIVATE USE_FFMPEG_STATIC)

    if(WIN32)
        # Priority 1: Check project-local ffmpeg-dev/ directory (setup-ffmpeg-dev.ps1)
        set(FFMPEG_DEV_DIR "${CMAKE_CURRENT_SOURCE_DIR}/ffmpeg-dev")
        if(EXISTS "${FFMPEG_DEV_DIR}/include/libavformat/avformat.h")
            message(STATUS "Found FFmpeg dev libraries in ffmpeg-dev/ (local)")
            target_include_directories(asmr-helper PRIVATE "${FFMPEG_DEV_DIR}/include")
            target_link_directories(asmr-helper PRIVATE "${FFMPEG_DEV_DIR}/lib")
            target_link_libraries(asmr-helper PRIVATE
                avformat avcodec avfilter avutil swscale swresample
                bcrypt secur32)
        else()
            # Priority 2: Try pkg-config
            find_package(PkgConfig QUIET)
            if(PKG_CONFIG_FOUND)
                pkg_check_modules(FFMPEG QUIET
                    libavformat libavcodec libavfilter libavutil libswscale libswresample)
                if(FFMPEG_FOUND)
                    message(STATUS "Found FFmpeg via pkg-config")
                    target_include_directories(asmr-helper PRIVATE ${FFMPEG_INCLUDE_DIRS})
                    target_link_directories(asmr-helper PRIVATE ${FFMPEG_LIBRARY_DIRS})
                    target_link_libraries(asmr-helper PRIVATE ${FFMPEG_LIBRARIES} bcrypt secur32)
                else()
                    # Priority 3: find_library fallback
                    find_path(FFMPEG_INCLUDE_DIR libavformat/avformat.h)
                    find_library(AVFORMAT_LIB avformat)
                    find_library(AVCODEC_LIB avcodec)
                    find_library(AVFILTER_LIB avfilter)
                    find_library(AVUTIL_LIB avutil)
                    find_library(SWSCALE_LIB swscale)
                    find_library(SWRESAMPLE_LIB swresample)
                    if(FFMPEG_INCLUDE_DIR AND AVFORMAT_LIB)
                        message(STATUS "Found FFmpeg via find_library")
                        target_include_directories(asmr-helper PRIVATE ${FFMPEG_INCLUDE_DIR})
                        target_link_libraries(asmr-helper PRIVATE
                            ${AVFORMAT_LIB} ${AVCODEC_LIB} ${AVFILTER_LIB}
                            ${AVUTIL_LIB} ${SWSCALE_LIB} ${SWRESAMPLE_LIB}
                            bcrypt secur32)
                    else()
                        message(FATAL_ERROR
                            "FFmpeg dev libraries not found.\n"
                            "Run this script to download them automatically:\n"
                            "  powershell -ExecutionPolicy Bypass -File scripts\\setup-ffmpeg-dev.ps1")
                    endif()
                endif()
            else()
                message(FATAL_ERROR
                    "FFmpeg dev libraries not found.\n"
                    "Run this script to download them automatically:\n"
                    "  powershell -ExecutionPolicy Bypass -File scripts\\setup-ffmpeg-dev.ps1")
            endif()
        endif()

        # Common Windows libs
        target_link_libraries(asmr-helper PRIVATE winhttp shlwapi ws2_32)
    else()
        # Unix/Linux: use pkg-config
        find_package(PkgConfig REQUIRED)
        pkg_check_modules(FFMPEG REQUIRED
            libavformat libavcodec libavfilter libavutil libswscale libswresample)
        target_include_directories(asmr-helper PRIVATE ${FFMPEG_INCLUDE_DIRS})
        target_link_directories(asmr-helper PRIVATE ${FFMPEG_LIBRARY_DIRS})
        target_link_libraries(asmr-helper PRIVATE ${FFMPEG_LIBRARIES} pthread m z)

        # CURL for HTTP
        find_package(CURL REQUIRED)
        target_link_libraries(asmr-helper PRIVATE CURL::libcurl)
    endif()
else()
    # Non-static mode: platform dependencies
    if(WIN32)
        # Windows: WinHTTP (built-in, no external dependency)
        target_link_libraries(asmr-helper PRIVATE winhttp shlwapi ws2_32)
    else()
        # Unix: CURL
        find_package(CURL REQUIRED)
        target_link_libraries(asmr-helper PRIVATE CURL::libcurl)
    endif()
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
