add_library(SerialPortTool STATIC)
target_sources(SerialPortTool PRIVATE SerialPort.h SerialPort.cpp)
target_link_libraries(SerialPortTool PUBLIC VICore VIEngine VIUtils Qt6::Core Qt6::Network Qt6::SerialPort)
