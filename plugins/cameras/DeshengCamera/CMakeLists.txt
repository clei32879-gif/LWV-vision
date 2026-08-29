# ?? DVP2 SDK ????
# ?? QLibrary ???? DVPCamera64. dll
# ??????????? SDK ???

qt_add_plugin(DeshengCamera STATIC)

target_sources(DeshengCamera PRIVATE
    DeshengCamera.h
    DeshengCamera.cpp
)

target_link_libraries(DeshengCamera PUBLIC
    VICore
    VIEngine
    VIHAL
    VIUtils
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)

message(STATUS "DeshengCamera: built-in type definitions")

