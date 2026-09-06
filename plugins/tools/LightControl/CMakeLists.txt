add_library(LightControl STATIC)
target_sources(LightControl PRIVATE LightControl.h LightControl.cpp)
target_link_libraries(LightControl PUBLIC VICore VIEngine VIUtils Qt6::Core Qt6::SerialPort)
