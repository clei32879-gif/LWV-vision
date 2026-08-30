add_library(RotatePoint STATIC)
target_sources(RotatePoint PRIVATE RotatePoint.h RotatePoint.cpp)
target_link_libraries(RotatePoint PUBLIC VICore VIEngine VIUtils Qt6::Core Qt6::Gui)

