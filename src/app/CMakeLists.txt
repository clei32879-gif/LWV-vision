# Main Application
qt_add_executable(VisionInspector
    main.cpp
    MainWindow.h
    MainWindow.cpp
    AppSettings.h
    AppSettings.cpp
)

target_link_libraries(VisionInspector PRIVATE
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
set(ALL_PLUGINS
    BlobAnalysis BrightnessCheck CircleDetection EdgeDetection
    ImageProcess LineDetection TemplateMatch
    GrayscaleMatch ShapeMatch ContourMatch
    BarcodeReader QRCodeReader OCR
    DistanceMeasure AngleMeasure Caliper
    ImageFilter Threshold Morphology ColorConvert
    ConditionBranch DataJudge Loop Delay
    CaptureImage DataDisplay UpdateView
    Calculator ImageCompare Calibration PositionCorrection
    VertexDetection WidthDetection MultiContourMatch ContourCompare ColorDetection
    CoordSystem ImageCorrection CalculateVariable SetVariable
    SerialPortTool EthernetTool ModbusRead ModbusWrite
    EndCorrection
)
foreach(_plugin ${ALL_PLUGINS})
    target_link_libraries(VisionInspector PRIVATE $<LINK_LIBRARY:WHOLE_ARCHIVE,${_plugin}>)
endforeach()

# Link DVP2 SDK
if(VI_HAS_DVP2)
    target_link_libraries(VisionInspector PRIVATE DVPCamera64)
    target_link_directories(VisionInspector PRIVATE ${DVP2_LIB_DIR})
endif()

if(MSVC)
    target_compile_options(VisionInspector PRIVATE $<$<CONFIG:Debug>:/Zi>)
    set_target_properties(VisionInspector PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

install(TARGETS VisionInspector RUNTIME DESTINATION bin)

if(VI_HAS_DVP2)
    add_custom_command(TARGET VisionInspector POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${DVP2_DLL_DIR}/DVPCamera64.dll" "$<TARGET_FILE_DIR:VisionInspector>"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${DVP2_DLL_DIR}/dvpir64.dll" "$<TARGET_FILE_DIR:VisionInspector>"
        COMMENT "Copying DVP2 DLLs...")
endif()
