add_library(EthernetTool STATIC)
target_sources(EthernetTool PRIVATE Ethernet.h Ethernet.cpp)
target_link_libraries(EthernetTool PUBLIC VICore VIEngine VIUtils Qt6::Core Qt6::Network)
