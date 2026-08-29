add_library(Caliper STATIC)
target_sources(Caliper PRIVATE Caliper.h Caliper.cpp)
target_link_libraries(Caliper PUBLIC VICore VIEngine VIUtils ${OpenCV_LIBS} Qt6::Core Qt6::Gui)
