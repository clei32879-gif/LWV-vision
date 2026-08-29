add_library(ModbusWrite STATIC)
target_sources(ModbusWrite PRIVATE ModbusWrite.h ModbusWrite.cpp)
target_link_libraries(ModbusWrite PUBLIC VICore VIEngine VIUtils Qt6::Core Qt6::Network)
