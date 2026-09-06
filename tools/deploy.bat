@echo off
REM ============================================================
REM LW Vision Release Packaging (ASCII only for cmd compatibility)
REM Product: D:\LWVision\release_pkg\LWVision\  (copy to target PC)
REM Usage: build release first (build.bat release), then run this
REM ============================================================
setlocal enabledelayedexpansion
cd /d "%~dp0.."

set "SRC=build\release\bin"
set "DST=release_pkg\LWVision"

if not exist "%SRC%\LWVision.exe" (
    echo [ERROR] %SRC%\LWVision.exe not found. Run build.bat release first.
    exit /b 1
)

echo [1/5] Prepare release dir ...
if exist "%DST%" rmdir /s /q "%DST%"
mkdir "%DST%" 2>nul

echo [2/5] Copy app + Qt runtime (windeployqt) ...
copy /y "%SRC%\LWVision.exe" "%DST%\" >nul
windeployqt --release --no-compiler-runtime "%DST%\LWVision.exe" >nul 2>&1
for %%F in (libgcc_s_seh-1.dll libstdc++-6.dll libwinpthread-1.dll) do (
    copy /y "C:\Qt\Tools\mingw1310_64\bin\%%F" "%DST%\" >nul 2>&1
)

echo [3/5] Copy OpenCV + ONNX Runtime ...
for %%F in (thirdparty\opencv\build\bin\libopencv_*.dll) do copy /y "%%F" "%DST%\" >nul
copy /y "%SRC%\onnxruntime.dll" "%DST%\" >nul 2>&1
copy /y "%SRC%\onnxruntime_providers_shared.dll" "%DST%\" >nul 2>&1
copy /y "%SRC%\DirectML.dll" "%DST%\" >nul 2>&1
if exist thirdparty\onnxruntime\lib\DirectML.dll copy /y thirdparty\onnxruntime\lib\DirectML.dll "%DST%\" >nul 2>&1

echo [4/5] Copy models + sample images ...
mkdir "%DST%\models" 2>nul
if exist build\release\bin\models\yolov8n.onnx copy /y build\release\bin\models\yolov8n.onnx "%DST%\models\" >nul
if exist testdata\runs\classify\train\weights\best.onnx copy /y testdata\runs\classify\train\weights\best.onnx "%DST%\models\defect_cls.onnx" >nul
if exist testdata\runs\classify\train\weights\classes.txt (
    copy /y testdata\runs\classify\train\weights\classes.txt "%DST%\models\defect_cls_classes.txt" >nul
    copy /y testdata\runs\classify\train\weights\classes.txt "%DST%\models\classes.txt" >nul
)
if exist "%SRC%\testdata\real_samples" (
    mkdir "%DST%\testdata\real_samples" 2>nul
    for %%F in (%SRC%\testdata\real_samples\*.png) do copy /y "%%F" "%DST%\testdata\real_samples\" >nul 2>&1
)
if exist "%SRC%\testdata\virtual_camera" (
    mkdir "%DST%\testdata\virtual_camera" 2>nul
    for %%F in (%SRC%\testdata\virtual_camera\*.png) do copy /y "%%F" "%DST%\testdata\virtual_camera\" >nul 2>&1
)

REM ---- Qt SQL driver plugin (SQLite persistence, must be in sqldrivers subdir) ----
REM (windeployqt usually copies it once exe links Qt6::Sql; explicit copy as safety net)
mkdir "%DST%\sqldrivers" 2>nul
if exist "C:\Qt\6.10.3\mingw_64\plugins\sqldrivers\qsqlite.dll" copy /y "C:\Qt\6.10.3\mingw_64\plugins\sqldrivers\qsqlite.dll" "%DST%\sqldrivers\" >nul 2>&1
if exist "%SRC%\sqldrivers\qsqlite.dll" copy /y "%SRC%\sqldrivers\qsqlite.dll" "%DST%\sqldrivers\" >nul 2>&1

REM ---- templates/ data/ dirs (template gallery and records db location) ----
mkdir "%DST%\templates" 2>nul
mkdir "%DST%\data" 2>nul

echo [5/5] Done.
echo.
echo ============================================================
echo  Package ready: %CD%\%DST%\
echo  Copy the whole folder to the target PC (Win10/11 x64).
echo  See docs\gongkongjibushu for deployment details (Chinese).
echo ============================================================
