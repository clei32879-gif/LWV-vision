add_library(CalculateVariable STATIC)
target_sources(CalculateVariable PRIVATE CalculateVariable.h CalculateVariable.cpp)
target_link_libraries(CalculateVariable PUBLIC VICore VIEngine VIUtils Qt6::Core)
