add_library(CoordSystem STATIC)
target_sources(CoordSystem PRIVATE CoordSystem.h CoordSystem.cpp)
target_link_libraries(CoordSystem PUBLIC VICore VIEngine VIUtils Qt6::Core)
