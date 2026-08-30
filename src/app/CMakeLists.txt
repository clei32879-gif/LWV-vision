# Main Application
qt_add_executable(LWVision
    main.cpp
    MainWindow.h
    MainWindow.cpp
    AppSettings.h
    AppSettings.cpp
)

target_link_libraries(LWVision PRIVATE
    VICore
    VIEngine
    VIHAL
    VIUtils
    VIUI
    ${OpenCV_LIBS}
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)

# Force whole-archive for plugin libs so static initializers (VI_REGISTER_TOOL) run
if(VI_HAS_ONNXRT)
    target_link_libraries(LWVision PRIVATE VIAI)
endif()
foreach(_plugin ${VI_ALL_PLUGINS})
    if(TARGET ${_plugin})
        target_link_libraries(LWVision PRIVATE $<LINK_LIBRARY:WHOLE_ARCHIVE,${_plugin}>)
    endif()
endforeach()

# Link DVP2 SDK
if(VI_HAS_DVP2)
    target_link_libraries(LWVision PRIVATE DVPCamera64)
    target_link_directories(LWVision PRIVATE ${DVP2_LIB_DIR})
endif()

# GUI 程序: 隐藏控制台窗口 (MinGW/MSVC 通用)
set_target_properties(LWVision PROPERTIES WIN32_EXECUTABLE TRUE)

if(MSVC)
    target_compile_options(LWVision PRIVATE $<$<CONFIG:Debug>:/Zi>)
endif()

install(TARGETS LWVision RUNTIME DESTINATION bin)

if(VI_HAS_DVP2)
    add_custom_command(TARGET LWVision POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${DVP2_DLL_DIR}/DVPCamera64.dll" "$<TARGET_FILE_DIR:LWVision>"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${DVP2_DLL_DIR}/dvpir64.dll" "$<TARGET_FILE_DIR:LWVision>"
        COMMENT "Copying DVP2 DLLs...")
endif()
