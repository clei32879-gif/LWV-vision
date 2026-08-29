add_library(VertexDetection STATIC)
target_sources(VertexDetection PRIVATE VertexDetection.h VertexDetection.cpp)
target_link_libraries(VertexDetection PUBLIC VICore VIEngine VIUtils ${OpenCV_LIBS} Qt6::Core Qt6::Gui)
