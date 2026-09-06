add_library(SelectBranch STATIC)
target_sources(SelectBranch PRIVATE SelectBranch.h SelectBranch.cpp)
target_link_libraries(SelectBranch PUBLIC VICore VIEngine VIUtils Qt6::Core)
