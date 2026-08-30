add_library(PlcLink STATIC)
target_sources(PlcLink PRIVATE PlcLink.h PlcLink.cpp)
target_link_libraries(PlcLink PUBLIC VICore VIEngine VIUtils VIHAL Qt6::Core Qt6::Network Qt6::SerialPort)
