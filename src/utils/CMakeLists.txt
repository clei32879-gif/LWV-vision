# VIUtils - Logger, JsonHelper, Common
qt_add_library(VIUtils STATIC)

target_sources(VIUtils PRIVATE
    Logger.h
    Logger.cpp
    JsonHelper.h
    JsonHelper.cpp
    Common.h
    Utils.h
    Utils.cpp
)

target_link_libraries(VIUtils PUBLIC
    Qt6::Core
    Qt6::Gui
)

target_include_directories(VIUtils PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
