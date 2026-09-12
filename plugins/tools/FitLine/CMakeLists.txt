add_library(FitLine STATIC)
target_sources(FitLine PRIVATE FitLine.h FitLine.cpp)
target_link_libraries(FitLine PUBLIC VICore VIEngine VIUtils ${OpenCV_LIBS} Qt6::Core)
