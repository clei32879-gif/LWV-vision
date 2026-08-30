# 测试
message(STATUS "tests: ONNXRT=${VI_HAS_ONNXRT}")
cmake_minimum_required(VERSION 3.21)

find_package(Qt6 REQUIRED COMPONENTS Concurrent)
add_executable(test_modbus test_modbus.cpp)
target_link_libraries(test_modbus PRIVATE
    VIHAL
    VIUtils
    ${OpenCV_LIBS}
    Qt6::Core
    Qt6::Network
    Qt6::Concurrent
)
add_test(NAME modbus_tcp COMMAND test_modbus)

add_executable(test_engine test_engine.cpp)
target_link_libraries(test_engine PRIVATE
    VIEngine
    VIUtils
    ${OpenCV_LIBS}
    Qt6::Core
    Qt6::Concurrent
)
add_test(NAME flow_engine COMMAND test_engine)

add_executable(test_edgedrawing test_edgedrawing.cpp)
target_link_libraries(test_edgedrawing PRIVATE
    ${OpenCV_LIBS}
    Qt6::Core
)
add_test(NAME edge_drawing COMMAND test_edgedrawing)

# 工具回归测试: 链全部插件(静态注册) + 引擎
add_executable(test_tools_regression test_tools_regression.cpp)
target_link_libraries(test_tools_regression PRIVATE
    VICore
    VIEngine
    VIHAL
    VIUtils
    ${OpenCV_LIBS}
    Qt6::Core
    Qt6::Gui
)
foreach(_plugin ${VI_ALL_PLUGINS})
    if(TARGET ${_plugin})
        target_link_libraries(test_tools_regression PRIVATE $<LINK_LIBRARY:WHOLE_ARCHIVE,${_plugin}>)
    endif()
endforeach()
add_test(NAME tools_regression COMMAND test_tools_regression)

# 螺纹检测回归: 链插件(静态注册)
add_executable(test_thread test_thread.cpp)
target_link_libraries(test_thread PRIVATE
    VICore
    VIEngine
    VIHAL
    VIUtils
    ${OpenCV_LIBS}
    Qt6::Core
    Qt6::Gui
)
foreach(_plugin ${VI_ALL_PLUGINS})
    if(TARGET ${_plugin})
        target_link_libraries(test_thread PRIVATE $<LINK_LIBRARY:WHOLE_ARCHIVE,${_plugin}>)
    endif()
endforeach()
add_test(NAME thread_inspection COMMAND test_thread)

# ONNX Runtime 最小链接验证 (可选依赖)
if(VI_HAS_ONNXRT)
    add_executable(test_onnxrt_min test_onnxrt_min.cpp)
    vi_link_onnxrt(test_onnxrt_min)
    add_test(NAME onnxrt_min COMMAND test_onnxrt_min)

    # AI推理功能测试 (链VIAI+全部插件)
    add_executable(test_ai test_ai.cpp)
    vi_link_onnxrt(test_ai)
    target_link_libraries(test_ai PRIVATE
        VIAI VICore VIEngine VIHAL VIUtils ${OpenCV_LIBS} Qt6::Core Qt6::Gui)
    foreach(_plugin ${VI_ALL_PLUGINS})
        if(TARGET ${_plugin})
            target_link_libraries(test_ai PRIVATE $<LINK_LIBRARY:WHOLE_ARCHIVE,${_plugin}>)
        endif()
    endforeach()
    add_test(NAME ai_inference COMMAND test_ai)
endif()
