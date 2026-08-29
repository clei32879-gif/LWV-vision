add_library(GrayscaleMatch STATIC)
target_sources(GrayscaleMatch PRIVATE GrayscaleMatch.h GrayscaleMatch.cpp)
target_link_libraries(GrayscaleMatch PUBLIC VICore VIEngine VIUtils ${OpenCV_LIBS} Qt6::Core Qt6::Gui)
