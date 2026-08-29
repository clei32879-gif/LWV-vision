add_library(ImageFilter STATIC)
target_sources(ImageFilter PRIVATE ImageFilter.h ImageFilter.cpp)
target_link_libraries(ImageFilter PUBLIC VICore VIEngine VIUtils ${OpenCV_LIBS} Qt6::Core Qt6::Gui)
