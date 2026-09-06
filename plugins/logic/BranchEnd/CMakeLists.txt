add_library(BranchEnd STATIC)
target_sources(BranchEnd PRIVATE BranchEnd.h BranchEnd.cpp)
target_link_libraries(BranchEnd PUBLIC VICore VIEngine VIUtils Qt6::Core)
