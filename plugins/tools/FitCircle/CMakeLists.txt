add_library(FitCircle STATIC)
target_sources(FitCircle PRIVATE FitCircle.h FitCircle.cpp)
target_link_libraries(FitCircle PUBLIC VICore VIEngine VIUtils ${OpenCV_LIBS} Qt6::Core)
