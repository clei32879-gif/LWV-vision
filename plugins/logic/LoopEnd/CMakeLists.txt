add_library(LoopEnd STATIC)
target_sources(LoopEnd PRIVATE LoopEnd.h LoopEnd.cpp)
target_link_libraries(LoopEnd PUBLIC VICore VIEngine VIUtils Qt6::Core)

