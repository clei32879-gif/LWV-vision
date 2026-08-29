cmake_minimum_required(VERSION 3.21)
project(TestOpenCV)

set(OpenCV_DIR "E:/guangxuan/thirdparty/opencv/build")
set(OpenCV_INCLUDE_DIRS "E:/guangxuan/thirdparty/opencv/build/include")
set(OpenCV_LIBRARY_DIRS "E:/guangxuan/thirdparty/opencv/build/lib")

find_package(OpenCV QUIET COMPONENTS core imgproc)

if(OpenCV_FOUND)
    message(STATUS "OpenCV_FOUND: ${OpenCV_FOUND}")
    message(STATUS "VI_HAS_OPENCV would be: ON")
else()
    message(STATUS "OpenCV_FOUND: ${OpenCV_FOUND}")
    message(STATUS "VI_HAS_OPENCV would be: OFF")
endif()
