# Hardware Abstraction Layer
qt_add_library(VIHAL STATIC)

target_sources(VIHAL PRIVATE
    # Camera driver interface
    ICameraDriver.h

    # Daheng DVP2 camera driver
    DeshengCamera.h
    DeshengCamera.cpp

    # Virtual camera driver (无硬件开发/演示)
    VirtualCamera.h
    VirtualCamera.cpp

    # PLC driver interface
    IPLCDriver.h

    # Servo driver interface
    IServoDriver.h

    # IO driver interface
    IIODriver.h

    # Hardware device manager
    HardwareManager.h
    HardwareManager.cpp
)

target_link_libraries(VIHAL PUBLIC
    VICore
    Qt6::Core
    Qt6::Network
)

# Link DVP2 SDK if available
if(VI_HAS_DVP2)
    target_link_libraries(VIHAL PRIVATE DVPCamera64)
    target_link_directories(VIHAL PRIVATE ${DVP2_LIB_DIR})
endif()

# SerialPort is optional
if(TARGET Qt6::SerialPort)
    target_link_libraries(VIHAL PUBLIC Qt6::SerialPort)
    target_compile_definitions(VIHAL PUBLIC VI_HAS_SERIALPORT)
endif()

target_include_directories(VIHAL PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
