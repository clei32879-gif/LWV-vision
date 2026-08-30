add_library(CompareText STATIC)
target_sources(CompareText PRIVATE CompareText.h CompareText.cpp)
target_link_libraries(CompareText PUBLIC VICore VIEngine VIUtils Qt6::Core)

