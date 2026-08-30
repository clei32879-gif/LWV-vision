add_library(GenerateText STATIC)
target_sources(GenerateText PRIVATE GenerateText.h GenerateText.cpp)
target_link_libraries(GenerateText PUBLIC VICore VIEngine VIUtils Qt6::Core)

