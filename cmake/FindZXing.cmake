# ============================================================
# zxing-cpp (条码/DM码/QR 一体识别, MIT) — 可选依赖
# 源码: thirdparty/zxing-cpp (v2.2.1 tarball)
# ============================================================
set(ZXING_SRC_DIR "${CMAKE_SOURCE_DIR}/thirdparty/zxing-cpp/core/src")
if(EXISTS "${ZXING_SRC_DIR}/ReadBarcode.h")
    message(STATUS "zxing-cpp found: ${ZXING_SRC_DIR}")
    set(VI_HAS_ZXING ON)
    include_directories(${ZXING_SRC_DIR})

    # 精简构建: 只编解码核心 (不放成库, 直接把源文件并入 VICore? — zxing 源文件量大,
    # 用静态库目标更干净; 关闭示例/测试/黑盒/写入器)
    set(BUILD_EXPERIMENTAL_API OFF CACHE BOOL "" FORCE)
    set(BUILD_READERS ON CACHE BOOL "" FORCE)
    set(BUILD_WRITERS OFF CACHE BOOL "" FORCE)
    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(BUILD_BLACKBOX_TESTS OFF CACHE BOOL "" FORCE)
    set(BUILD_UNIT_TESTS OFF CACHE BOOL "" FORCE)
    add_subdirectory(thirdparty/zxing-cpp/core ZXingCore)
else()
    message(STATUS "zxing-cpp NOT found (thirdparty/zxing-cpp/core/src/ReadBarcode.h missing)")
    set(VI_HAS_ZXING OFF)
endif()
