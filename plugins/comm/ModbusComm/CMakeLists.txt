# ModbusComm 插件
qt_add_plugin(ModbusComm STATIC)
target_sources(ModbusComm PRIVATE
    ModbusComm.h
    ModbusComm.cpp
)
target_link_libraries(ModbusComm PUBLIC
    VICore
    VIEngine
    VIHAL
    VIUtils
    ${OpenCV_LIBS}
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)
