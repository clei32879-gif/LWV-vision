add_library(MultiContourMatch STATIC)
target_sources(MultiContourMatch PRIVATE MultiContourMatch.h MultiContourMatch.cpp)
target_link_libraries(MultiContourMatch PUBLIC VICore VIEngine VIUtils ${OpenCV_LIBS} Qt6::Core Qt6::Gui)
