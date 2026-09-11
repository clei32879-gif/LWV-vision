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
# ScriptNode 插件用 Qt6::Qml (QJSEngine), 测试同样需要链接
if(TARGET Qt6::Qml)
    target_link_libraries(test_engine PRIVATE Qt6::Qml)
    target_compile_definitions(test_engine PRIVATE VI_HAS_QJS)
endif()
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

# 括号式位置补正回归 (位置补正→结束补正区间内自动跟随, 免勾选)
add_executable(test_bracket_correction test_bracket_correction.cpp)
target_link_libraries(test_bracket_correction PRIVATE
    VIEngine
    VIUtils
    ${OpenCV_LIBS}
    Qt6::Core
    Qt6::Concurrent
)
# 工具注册依赖插件静态对象初始化, 必须 WHOLE_ARCHIVE 全量链入
foreach(_plugin ${VI_ALL_PLUGINS})
    if(TARGET ${_plugin})
        target_link_libraries(test_bracket_correction PRIVATE $<LINK_LIBRARY:WHOLE_ARCHIVE,${_plugin}>)
    endif()
endforeach()
add_test(NAME bracket_correction COMMAND test_bracket_correction)

# 授权系统回归 (阶段6: 密钥对/验签/篡改检测/机器码)
add_executable(test_license test_license.cpp)
target_link_libraries(test_license PRIVATE
    VICore
    VIEngine
    VIHAL
    VIUtils
    Qt6::Core
    Qt6::Network
)
if(WIN32)
    target_link_libraries(test_license PRIVATE bcrypt)
endif()
add_test(NAME license COMMAND test_license)

# 内置设备模板回归 (阶段6: 工具注册齐全/无头构建/实例名去重)
add_executable(test_device_templates test_device_templates.cpp)
target_link_libraries(test_device_templates PRIVATE
    VICore
    VIEngine
    VIUtils
    Qt6::Core
    Qt6::Concurrent
)
# 工具注册依赖插件静态对象的静态初始化, 必须 WHOLE_ARCHIVE 全量链入
foreach(_plugin ${VI_ALL_PLUGINS})
    if(TARGET ${_plugin})
        target_link_libraries(test_device_templates PRIVATE $<LINK_LIBRARY:WHOLE_ARCHIVE,${_plugin}>)
    endif()
endforeach()
add_test(NAME device_templates COMMAND test_device_templates)

# 检测记录SQLite持久化回归 (阶段6: 写入/查询/过滤/导出/清理)
add_executable(test_detection_recorder test_detection_recorder.cpp)
target_link_libraries(test_detection_recorder PRIVATE
    VIEngine
    VIUtils
    Qt6::Core
    Qt6::Sql
)
# SQLite驱动插件需在 <exeDir>/sqldrivers/ 下才能被Qt加载
if(TARGET Qt6::QSQLiteDriverPlugin)
    add_custom_command(TARGET test_detection_recorder POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:test_detection_recorder>/sqldrivers"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:Qt6::QSQLiteDriverPlugin>"
            "$<TARGET_FILE_DIR:test_detection_recorder>/sqldrivers/")
endif()
add_test(NAME detection_recorder COMMAND test_detection_recorder)

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
    Qt6::Network
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

# OCR 真实功能验证 (教导→识别→泛化)
add_executable(test_ocr test_ocr.cpp)
target_link_libraries(test_ocr PRIVATE
    VICore VIEngine VIHAL VIUtils ${OpenCV_LIBS} Qt6::Core Qt6::Gui)
foreach(_plugin ${VI_ALL_PLUGINS})
    if(TARGET ${_plugin})
        target_link_libraries(test_ocr PRIVATE $<LINK_LIBRARY:WHOLE_ARCHIVE,${_plugin}>)
    endif()
endforeach()
add_test(NAME ocr COMMAND test_ocr)

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

# GigE 相机网络诊断工具 (纯WinSock, 无Qt依赖; 现场排查GVCP连通性)
add_executable(test_gige_network test_gige_network.cpp)
if(WIN32)
    target_link_libraries(test_gige_network PRIVATE ws2_32 iphlpapi)
endif()
