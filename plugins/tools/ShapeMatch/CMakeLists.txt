add_library(ShapeMatch STATIC)
target_sources(ShapeMatch PRIVATE ShapeMatch.h ShapeMatch.cpp)
target_link_libraries(ShapeMatch PUBLIC VICore VIEngine VIUtils ${OpenCV_LIBS} Qt6::Core Qt6::Gui)
