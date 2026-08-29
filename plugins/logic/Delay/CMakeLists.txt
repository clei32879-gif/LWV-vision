add_library(Delay STATIC)
target_sources(Delay PRIVATE Delay.h Delay.cpp)
target_link_libraries(Delay PUBLIC VICore VIEngine VIUtils Qt6::Core)
