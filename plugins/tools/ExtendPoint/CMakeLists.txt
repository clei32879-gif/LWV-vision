add_library(ExtendPoint STATIC)
target_sources(ExtendPoint PRIVATE ExtendPoint.h ExtendPoint.cpp)
target_link_libraries(ExtendPoint PUBLIC VICore VIEngine VIUtils Qt6::Core Qt6::Gui)

