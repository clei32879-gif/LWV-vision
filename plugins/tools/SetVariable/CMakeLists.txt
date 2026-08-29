add_library(SetVariable STATIC)
target_sources(SetVariable PRIVATE SetVariable.h SetVariable.cpp)
target_link_libraries(SetVariable PUBLIC VICore VIEngine VIUtils Qt6::Core)
