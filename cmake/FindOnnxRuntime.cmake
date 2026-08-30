# ============================================================
# ONNX Runtime (AI推理, 可选依赖)
# 随项目携带: thirdparty/onnxruntime (头文件+导入库入库, DLL不入库)
# ============================================================
set(ONNXRT_DIR "${CMAKE_SOURCE_DIR}/thirdparty/onnxruntime")
if(EXISTS "${ONNXRT_DIR}/lib/onnxruntime.lib")
    set(VI_HAS_ONNXRT ON)
    message(STATUS "ONNX Runtime found: ${ONNXRT_DIR}")

    # MinGW: 强制前置SAL兼容头 (ORT头文件使用MSVC SAL注解)
    function(vi_link_onnxrt target)
        target_include_directories(${target} PRIVATE ${ONNXRT_DIR}/include)
        target_compile_definitions(${target} PRIVATE VI_HAS_ONNXRT)
        target_link_libraries(${target} PRIVATE ${ONNXRT_DIR}/lib/onnxruntime.lib)
        if(MINGW)
            target_compile_options(${target} PRIVATE
                "-include${CMAKE_SOURCE_DIR}/cmake/onnxrt_mingw_compat.h")
        endif()
        # 运行需要 onnxruntime.dll, 构建后拷到bin
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${ONNXRT_DIR}/lib/onnxruntime.dll"
                "$<TARGET_FILE_DIR:${target}>"
            COMMENT "Copying onnxruntime.dll...")
    endfunction()
else()
    set(VI_HAS_ONNXRT OFF)
    message(STATUS "ONNX Runtime NOT found (AI功能编译跳过): ${ONNXRT_DIR}")
endif()
