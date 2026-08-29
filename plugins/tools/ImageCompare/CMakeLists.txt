add_library(ImageCompare STATIC)
target_sources(ImageCompare PRIVATE ImageCompare.h ImageCompare.cpp)
target_link_libraries(ImageCompare PUBLIC VICore VIEngine VIUtils ${OpenCV_LIBS} Qt6::Core Qt6::Gui)
