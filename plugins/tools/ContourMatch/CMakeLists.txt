add_library(ContourMatch STATIC)
target_sources(ContourMatch PRIVATE ContourMatch.h ContourMatch.cpp)
target_link_libraries(ContourMatch PUBLIC VICore VIEngine VIUtils ${OpenCV_LIBS} Qt6::Core Qt6::Gui)
