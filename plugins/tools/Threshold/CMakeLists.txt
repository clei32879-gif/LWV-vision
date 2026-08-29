add_library(Threshold STATIC)
target_sources(Threshold PRIVATE Threshold.h Threshold.cpp)
target_link_libraries(Threshold PUBLIC VICore VIEngine VIUtils ${OpenCV_LIBS} Qt6::Core Qt6::Gui)
