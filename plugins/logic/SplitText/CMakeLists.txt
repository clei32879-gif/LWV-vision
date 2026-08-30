add_library(SplitText STATIC)
target_sources(SplitText PRIVATE SplitText.h SplitText.cpp)
target_link_libraries(SplitText PUBLIC VICore VIEngine VIUtils Qt6::Core)

