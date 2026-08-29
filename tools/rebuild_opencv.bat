@echo off
REM ============================================================
REM OpenCV 重建脚本 (加入 opencv_contrib/ximgproc 的 EdgeDrawing)
REM 用途: 更换编译器/升级OpenCV后重建随项目的OpenCV库
REM 产物: thirdparty/opencv/build (含ximgproc + calib)
REM 注意: 需要网络(下载opencv_contrib); 全程约20-40分钟
REM ============================================================
setlocal enabledelayedexpansion
chcp 65001 >nul
cd /d "%~dp0..\thirdparty\opencv"

set "OPENCV_VER=5.0.0"
set "QT_ROOT=C:/Qt/6.10.3/mingw_64"
set "PATH=%QT_ROOT%\bin;C:\Qt\Tools\mingw1310_64\bin;%PATH%"

echo [1/5] 下载 opencv_contrib %OPENCV_VER% ...
if not exist opencv_contrib-%OPENCV_VER%\modules (
    curl -L --retry 3 -o opencv_contrib-%OPENCV_VER%.zip https://codeload.github.com/opencv/opencv_contrib/zip/refs/tags/%OPENCV_VER% || goto :fail
    tar -xf opencv_contrib-%OPENCV_VER%.zip || goto :fail
)

echo [2/5] 备份旧build ...
if exist build if not exist build_backup ren build build_backup

echo [3/5] 配置 (禁用所有联网下载组件) ...
cmake -S opencv-%OPENCV_VER% -B build_new -G "MinGW Makefiles" ^
  -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=ON ^
  "-DBUILD_LIST=core,imgproc,imgcodecs,objdetect,features,flann,geometry,stereo,ptcloud,calib,ximgproc" ^
  -DOPENCV_EXTRA_MODULES_PATH=opencv_contrib-%OPENCV_VER%/modules ^
  -DBUILD_TESTS=OFF -DBUILD_PERF_TESTS=OFF -DBUILD_EXAMPLES=OFF ^
  -DBUILD_opencv_world=OFF -DBUILD_opencv_apps=OFF -DBUILD_opencv_gapi=OFF ^
  -DWITH_ADE=OFF -DWITH_IPP=OFF -DWITH_FFMPEG=OFF -DOPENCV_ENABLE_NONFREE=OFF
if errorlevel 1 goto :fail

echo [4/5] 编译 (12线程, 约20-40分钟) ...
mingw32-make -C build_new -j 12
if errorlevel 1 goto :fail

echo [5/5] 组装头文件并就位 ...
if exist build_new\include rmdir /s /q build_new\include
mkdir build_new\include
REM 头文件来源1: 旧build的完整头文件(源码包的include目录被精简过)
if exist build_backup\include xcopy /e /i /q build_backup\include build_new\include
REM 头文件来源2: contrib/主模块源码中新增模块的头
xcopy /e /i /q opencv_contrib-%OPENCV_VER%\modules\ximgproc\include\opencv2\ximgproc build_new\include\opencv2\ximgproc
copy /y opencv_contrib-%OPENCV_VER%\modules\ximgproc\include\opencv2\ximgproc.hpp build_new\include\opencv2\
copy /y opencv-%OPENCV_VER%\modules\calib\include\opencv2\calib.hpp build_new\include\opencv2\
copy /y opencv-%OPENCV_VER%\modules\calib\include\opencv2\calib3d.hpp build_new\include\opencv2\
xcopy /e /i /q opencv-%OPENCV_VER%\modules\calib\include\opencv2\calib3d build_new\include\opencv2\calib3d
copy /y opencv-%OPENCV_VER%\modules\stereo\include\opencv2\stereo.hpp build_new\include\opencv2\
REM 生成的配置头
copy /y build_new\cvconfig.h build_new\include\opencv2\
copy /y build_new\cv_cpu_config.h build_new\include\opencv2\ 2>nul
REM 可移植的CMake配置文件(项目手工维护,见库文件清单)
copy /y build_backup\OpenCVConfig.cmake build_new\ 2>nul
copy /y build_backup\OpenCVConfig-version.cmake build_new\ 2>nul
copy /y build_backup\OpenCVModules.cmake build_new\ 2>nul

move build_new build >nul
echo.
echo 完成! OpenCV(含ximgproc)已就位: thirdparty\opencv\build
echo 旧目录保留为 build_backup, 验证无误后可删除
exit /b 0

:fail
echo 失败! 详细日志见 configure_log.txt / 控制台输出
exit /b 1
