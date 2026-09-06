# 工具引擎 - 工具基类、流程执行引擎、工具注册表

qt_add_library(VIEngine STATIC)

target_sources(VIEngine PRIVATE
    # 工具基类接口
    ITool.h
    ITool.cpp

    # 工具上下文 (工具执行时的环境)
    ToolContext.h
    ToolContext.cpp

    # 工具注册表
    ToolRegistry.h
    ToolRegistry.cpp

    # 流程执行引擎
    FlowEngine.h
    FlowEngine.cpp

    # 数据模型
    DataModel.h
    DataModel.cpp

    # 检测结果数据
    InspectionResult.h
    InspectionResult.cpp

    # 检测记录SQLite持久化 (阶段6, 可选依赖Qt6::Sql)
    DetectionRecorder.h
    DetectionRecorder.cpp

    # ROI区域定义
    ROI.h
    ROI.cpp

    # 数据判断系统
    DataJudgment.h
    DataJudgment.cpp

    # 亚像素边缘测量库 (阶段2: 测量类工具共享)
    SubpixEdge.h
    SubpixEdge.cpp
)

target_link_libraries(VIEngine PUBLIC
    VICore
    ${OpenCV_LIBS}
    Qt6::Core
    Qt6::Gui
    Qt6::Concurrent
)

# 检测记录持久化: 有Qt6::Sql则启用 (DetectionRecorder, 阶段6)
if(TARGET Qt6::Sql)
    target_link_libraries(VIEngine PUBLIC Qt6::Sql)
    target_compile_definitions(VIEngine PUBLIC VI_HAS_QTSQL)
endif()

target_include_directories(VIEngine PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
