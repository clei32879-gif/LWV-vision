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
    Qt6::SerialPort
    Qt6::Concurrent
)
add_test(NAME modbus_tcp COMMAND test_modbus)

add_executable(test_engine test_engine.cpp)
target_link_libraries(test_engine PRIVATE
    VICore
    VIEngine
    VIHAL
    VIUtils
    ${OpenCV_LIBS}
    Qt6::Core
    Qt6::Concurrent
)
foreach(_plugin ${VI_ALL_PLUGINS})
    if(TARGET ${_plugin})
        target_link_libraries(test_engine PRIVATE $<LINK_LIBRARY:WHOLE_ARCHIVE,${_plugin}>)
    endif()
endforeach()
add_test(NAME flow_engine COMMAND test_engine)

# 检测结果统计/记录/CSV导出回归
add_executable(test_stats test_stats.cpp)
target_link_libraries(test_stats PRIVATE
    VIEngine
    VIUtils
    ${OpenCV_LIBS}
    Qt6::Core
    Qt6::Concurrent
)
add_test(NAME stats_export COMMAND test_stats)

# 用户管理密码安全存储回归 (H-7: 哈希落盘/旧明文升级/登录)
add_executable(test_usermanager test_usermanager.cpp)
target_link_libraries(test_usermanager PRIVATE
    VICore
    VIEngine
    VIHAL
    VIUtils
    Qt6::Core
)
add_test(NAME user_manager COMMAND test_usermanager)

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

        # 项目保存/加载往返QA
    add_executable(test_project_roundtrip test_project_roundtrip.cpp)
    target_link_libraries(test_project_roundtrip PRIVATE
        VICore VIEngine VIHAL VIUtils ${OpenCV_LIBS} Qt6::Core Qt6::Gui)
    foreach(_plugin ${VI_ALL_PLUGINS})
        if(TARGET ${_plugin})
            target_link_libraries(test_project_roundtrip PRIVATE $<LINK_LIBRARY:WHOLE_ARCHIVE,${_plugin}>)
        endif()
    endforeach()
    add_test(NAME project_roundtrip COMMAND test_project_roundtrip)

    # 自训缺陷分类模型验证
    add_executable(test_cls_model test_cls_model.cpp)
    vi_link_onnxrt(test_cls_model)
    target_link_libraries(test_cls_model PRIVATE
        VIAI VICore VIEngine VIUtils  Qt6::Core Qt6::Gui)
    add_test(NAME cls_model COMMAND test_cls_model)

    # 发布包环境 AI 真实推理验证 (质检2: 分类+检测, 读发布包 models/testdata)
    add_executable(test_pkg_infer test_pkg_infer.cpp)
    vi_link_onnxrt(test_pkg_infer)
    target_link_libraries(test_pkg_infer PRIVATE
        VIAI VICore VIEngine VIUtils ${OpenCV_LIBS} Qt6::Core Qt6::Gui)
    add_test(NAME pkg_infer COMMAND test_pkg_infer)

    # 教导向导端到端QA (真实训练, 较慢)
    add_executable(test_teach_flow test_teach_flow.cpp)
    vi_link_onnxrt(test_teach_flow)
    target_link_libraries(test_teach_flow PRIVATE
        VIAI VICore VIEngine VIUtils ${OpenCV_LIBS} Qt6::Core Qt6::Gui)
    add_test(NAME teach_flow COMMAND test_teach_flow)
endif()

# 图标加载回归: 遍历全部工具/分类, 验证素材图标非空 (链VIUI获取编译进的icons.qrc)
add_executable(test_icons test_icons.cpp)
target_link_libraries(test_icons PRIVATE
    VIUI
    VICore
    VIEngine
    VIHAL
    VIUtils
    ${OpenCV_LIBS}
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)
foreach(_plugin ${VI_ALL_PLUGINS})
    if(TARGET ${_plugin})
        target_link_libraries(test_icons PRIVATE $<LINK_LIBRARY:WHOLE_ARCHIVE,${_plugin}>)
    endif()
endforeach()
add_test(NAME icons COMMAND test_icons)
