add_library(TcpData STATIC)
target_sources(TcpData PRIVATE TcpData.h TcpData.cpp)
target_link_libraries(TcpData PUBLIC VICore VIEngine VIUtils Qt6::Core Qt6::Network)
