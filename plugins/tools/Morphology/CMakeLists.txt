add_library(Morphology STATIC)
target_sources(Morphology PRIVATE Morphology.h Morphology.cpp)
target_link_libraries(Morphology PUBLIC VICore VIEngine VIUtils ${OpenCV_LIBS} Qt6::Core Qt6::Gui)
