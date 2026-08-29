cmake_minimum_required(VERSION 3.21)

project(LWVision
    VERSION 1.0.0
    LANGUAGES CXX
)

# ============================================================
# Global Settings
# ============================================================
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(MINGW)
    set(CMAKE_CXX_EXTENSIONS ON)
else()
    set(CMAKE_CXX_EXTENSIONS OFF)
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

if(WIN32)
    set(CMAKE_WINDOWS_CODEPAGE 65001)
endif()

# ============================================================
# Options
# ============================================================
option(VI_BUILD_TESTS "Build tests" OFF)
option(VI_BUILD_PLUGINS "Build plugins" ON)

# ============================================================
# Third-party: nlohmann_json (header-only)
# ============================================================
add_library(nlohmann_json INTERFACE)
target_include_directories(nlohmann_json INTERFACE ${CMAKE_SOURCE_DIR}/thirdparty/nlohmann)
add_library(nlohmann_json::nlohmann_json ALIAS nlohmann_json)

# ============================================================
# OpenCV (随项目携带的MinGW编译版，路径相对于项目根目录，换电脑无需修改)
# ============================================================
set(OpenCV_DIR "${CMAKE_SOURCE_DIR}/thirdparty/opencv/build")
set(OpenCV_INCLUDE_DIRS "${OpenCV_DIR}/include")
set(OpenCV_LIBRARY_DIRS "${OpenCV_DIR}/lib")

# ============================================================
# DVP2 SDK (度申相机，可选；未安装时以模拟模式编译)
# 优先找系统安装目录，其次找项目 thirdparty 目录
# ============================================================
set(DVP2_SDK_DIR "C:/Program Files (x86)/DVP2 SDK CN/library/Visual C++")
set(DVP2_INCLUDE_DIR "${DVP2_SDK_DIR}/include")
set(DVP2_LIB_DIR "${DVP2_SDK_DIR}/lib/x64")
set(DVP2_DLL_DIR "${DVP2_SDK_DIR}/bin/x64")

# Find DVP2 SDK
if(EXISTS "${DVP2_INCLUDE_DIR}/DVPCamera.h")
    message(STATUS "DVP2 SDK found: ${DVP2_SDK_DIR}")
    set(VI_HAS_DVP2 ON)
    include_directories(${DVP2_INCLUDE_DIR})
    link_directories(${DVP2_LIB_DIR})
else()
    message(WARNING "DVP2 SDK not found at ${DVP2_SDK_DIR}")
    set(VI_HAS_DVP2 OFF)
endif()

# ============================================================
# Find Qt
# ============================================================
find_package(Qt6 6.2 REQUIRED COMPONENTS
    Core
    Gui
    Widgets
    Network
    Concurrent
)

find_package(Qt6 QUIET COMPONENTS
    SerialPort
    Sql
    OpenGL
    OpenGLWidgets
)

# ============================================================
# Find OpenCV (optional, 路径已在上方设置为项目内 thirdparty)
# ============================================================
find_package(OpenCV QUIET COMPONENTS core imgproc)

if(OpenCV_FOUND)
    message(STATUS "OpenCV found: ${OpenCV_VERSION}")
    set(VI_HAS_OPENCV ON)
    message(STATUS "DEBUG: VI_HAS_OPENCV set to ON")
    include_directories(${OpenCV_INCLUDE_DIRS})
    # MinGW版本OpenCV (含contrib的ximgproc/EdgeDrawing与calib标定)
    if(MINGW)
        set(OpenCV_LIBS "opencv_core500;opencv_imgproc500;opencv_imgcodecs500;opencv_objdetect500;opencv_features500;opencv_flann500;opencv_geometry500;opencv_ximgproc500;opencv_calib500")
        link_directories(${OpenCV_LIBRARY_DIRS})
    endif()
else()
    message(WARNING "OpenCV not found! Building without OpenCV (limited functionality).")
    set(VI_HAS_OPENCV OFF)
    set(OpenCV_LIBS "")
endif()

# ============================================================
# Include Paths
# ============================================================
include_directories(
    ${CMAKE_SOURCE_DIR}/src
)

if(OpenCV_INCLUDE_DIRS)
    include_directories(${OpenCV_INCLUDE_DIRS})
endif()

# Global compile definitions
if(VI_HAS_OPENCV)
    add_compile_definitions(VI_HAS_OPENCV)
endif()

if(VI_HAS_DVP2)
    add_compile_definitions(VI_HAS_DVP2)
endif()

# ============================================================
# Compiler Options
# ============================================================
if(MSVC)
    add_compile_options(/utf-8 /W3 /wd4819)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS NOMINMAX)
endif()

if(MINGW)
    add_compile_options(-Wall -Wno-unused-parameter)
    add_link_options(-static-libgcc -static-libstdc++)
endif()

# ============================================================
# Subdirectories
# ============================================================
add_subdirectory(src/core)
add_subdirectory(src/engine)
add_subdirectory(src/hal)
add_subdirectory(src/utils)
add_subdirectory(src/ui)
add_subdirectory(src/app)
add_subdirectory(tools)

# 测试图片生成: 编译后自动在bin目录生成虚拟相机回放图
add_custom_target(generate_testdata
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/testdata/virtual_camera
    COMMAND $<TARGET_FILE:testdata_gen> ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/testdata/virtual_camera 24 6
    DEPENDS testdata_gen
    COMMENT "生成虚拟相机测试图片..."
)

if(VI_BUILD_PLUGINS)
    add_subdirectory(plugins)
endif()

if(VI_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# ============================================================
# Build Info
# ============================================================
message(STATUS "")
message(STATUS "========== LW Vision 立维视觉 Build Config ==========")
message(STATUS "  Version:        ${PROJECT_VERSION}")
message(STATUS "  C++ Standard:   ${CMAKE_CXX_STANDARD}")
message(STATUS "  Qt Version:     ${Qt6_VERSION}")
message(STATUS "  OpenCV Version: ${OpenCV_VERSION}")
message(STATUS "  DVP2 SDK:       ${VI_HAS_DVP2}")
message(STATUS "  Compiler:       ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "  Platform:       ${CMAKE_SYSTEM_NAME} ${CMAKE_SYSTEM_PROCESSOR}")
message(STATUS "  Build Plugins:  ${VI_BUILD_PLUGINS}")
message(STATUS "  Build Tests:    ${VI_BUILD_TESTS}")
message(STATUS "==================================================")
message(STATUS "")
