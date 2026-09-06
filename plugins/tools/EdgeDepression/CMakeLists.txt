add_library(EdgeDepression STATIC)
target_sources(EdgeDepression PRIVATE EdgeDepression.h EdgeDepression.cpp)
target_link_libraries(EdgeDepression PUBLIC VICore VIEngine VIUtils ${OpenCV_LIBS} Qt6::Core)
