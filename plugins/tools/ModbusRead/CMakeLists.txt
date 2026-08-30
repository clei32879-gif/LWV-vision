add_library(ModbusRead STATIC)
target_sources(ModbusRead PRIVATE ModbusRead.h ModbusRead.cpp)
target_link_libraries(ModbusRead PUBLIC VICore VIEngine VIUtils VIHAL Qt6::Core Qt6::Network Qt6::SerialPort)
