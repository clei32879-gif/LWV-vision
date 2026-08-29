# 界面组件 - 菜单栏、工具栏、显示区域、流程编辑器等

qt_add_library(VIUI STATIC)

target_sources(VIUI PRIVATE
    # 菜单栏
    MenuBar.h
    MenuBar.cpp

    # 主工具栏
    MainToolBar.h
    MainToolBar.cpp

    # 显示区域 (可编辑的运行界面)
    DisplayArea.h
    DisplayArea.cpp

    # 流程编辑器 (基于QtNodeEditor, 后续集成)
    FlowEditor.h
    FlowEditor.cpp

    # 工具箱面板
    Toolbox.h
    Toolbox.cpp

    # 数据栏
    DataPanel.h
    DataPanel.cpp

    # 日志栏
    LogPanel.h
    LogPanel.cpp

    # 属性编辑对话框
    PropertyDialog.h
    PropertyDialog.cpp

    # PLC模拟器对话框 (Modbus TCP从站)
    PlcSimulatorDialog.h
    PlcSimulatorDialog.cpp

    # 界面编辑器 (DIY)
    UIEditor.h
    UIEditor.cpp

    # 自定义控件
    widgets/ImageWidget.h
    widgets/ImageWidget.cpp
    widgets/StatusWidget.h
    widgets/StatusWidget.cpp
    widgets/ButtonWidget.h
    widgets/ButtonWidget.cpp
    widgets/ValueWidget.h
    widgets/ValueWidget.cpp
    widgets/StatusPanel.h
    widgets/StatusPanel.cpp
    widgets/MultiViewWidget.h
    widgets/MultiViewWidget.cpp
)

target_link_libraries(VIUI PUBLIC
    VICore
    VIEngine
    VIHAL
    ${OpenCV_LIBS}
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
)

# OpenGL可选
if(TARGET Qt6::OpenGL AND TARGET Qt6::OpenGLWidgets)
    target_link_libraries(VIUI PUBLIC Qt6::OpenGL Qt6::OpenGLWidgets)
    target_compile_definitions(VIUI PUBLIC VI_HAS_OPENGL)
endif()

target_include_directories(VIUI PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
