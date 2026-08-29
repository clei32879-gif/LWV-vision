# SerialPort 插件
qt_add_plugin(SerialPort STATIC)
target_sources(SerialPort PRIVATE
    SerialPort.h
    SerialPort.cpp
)
target_link_libraries(SerialPort PUBLIC
    VICore
    VIEngine
    VIHAL
    VIUtils
    ${OpenCV_LIBS}
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)
