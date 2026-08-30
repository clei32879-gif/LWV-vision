# LW Vision — 编译部署指南

> 本文档是唯一的权威编译文档。工程路径可移植：**项目放在任何盘符、任何电脑都能编译**，无需修改任何文件。
> （原 SETUP.md / docs/Windows开发环境搭建.md 为历史文档，已合并到本文）

## 一、环境要求

| 软件 | 版本 | 说明 |
|------|------|------|
| **Qt** | 6.2+ （验证过 6.10.3 / 6.11.1） | 在线安装器勾选 **MinGW 组件**（如 MinGW 13.1.0 64-bit） |
| **CMake** | 3.21+ | Qt 自带的 CMake 即可（Qt/Tools/CMake_64） |
| **MinGW** | 随 Qt 安装 | Qt/Tools/mingw13xx_64 |
| **OpenCV** | 5.0.0 MinGW 编译版 | **项目已自带**（thirdparty/opencv/build），无需安装 |

### 可选组件

| 组件 | 用途 | 说明 |
|------|------|------|
| DVP2 SDK（度申相机） | 真实相机采集 | 安装到 `C:\Program Files (x86)\DVP2 SDK CN`；未安装时程序以无相机模式编译，不影响其他功能 |
| ONNX Runtime (DirectML) | YOLOv8 AI 推理 / GPU 加速 | 项目已自带 DirectML 版 ONNX Runtime（thirdparty/onnxruntime，含 DirectML.dll）；运行时优先 DirectML(GPU)，不可用时自动回退 CPU |

## 二、编译（三选一）

### 方式A：一键脚本（推荐）

双击 `build.bat`，或命令行执行：

```bat
build.bat          :: Debug 版
build.bat release  :: Release 版
```

脚本自动探测 Qt 安装位置（支持 C/D/E/F 盘），配置并编译，产物在：

```
build\default\bin\LWVision.exe   (Debug)
build\release\bin\LWVision.exe   (Release)
```

### 方式B：命令行手动

```bat
:: QT_ROOT_DIR 指向 Qt 的 mingw_64 目录（按实际安装路径调整）
set QT_ROOT_DIR=C:\Qt\6.10.3\mingw_64
set PATH=%QT_ROOT_DIR%\bin;C:\Qt\Tools\mingw1310_64\bin;%PATH%

cmake --preset default
cmake --build build/default -j
```

### 方式C：Qt Creator

1. 文件 → 打开文件或项目 → 选 `CMakeLists.txt`
2. 构建套件选 **Qt 6.x MinGW 64-bit**
3. 构建目录设为 `build/default`（与预设一致）
4. Ctrl+B 构建，Ctrl+R 运行

## 三、OpenCV 说明

- 项目内 `thirdparty/opencv/build` 是随项目携带的 **MinGW 编译版 OpenCV 5.0.0**，CMake 自动按项目相对路径找到它，**换电脑无需任何操作**。
- 如需重新编译 OpenCV（例如换编译器版本）：

```bat
cd thirdparty\opencv\opencv-5.0.0
mkdir build_mingw && cd build_mingw
cmake .. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=ON -DBUILD_TESTS=OFF -DBUILD_PERF_TESTS=OFF -DBUILD_EXAMPLES=OFF
mingw32-make -j8 && mingw32-make install
:: 将 install 目录内容同步回 thirdparty\opencv\build
```

> ⚠️ MSVC 预编译版 OpenCV 与 MinGW ABI 不兼容，必须用 MinGW 自行编译。

## 四、相机支持

- **DVP2 SDK（度申）**：安装后重新编译即自动启用（CMake 输出中 `DVP2 SDK: ON`）。
- 未安装 SDK 时：相机菜单可打开但扫描不到设备，其余功能正常。
- 海康 MVS / 巴斯勒 Pylon：驱动接口已预留（`src/hal/ICameraDriver.h`），插件待实现。

## 五、部署到工控机

```bat
:: 1. 编译 Release 版
build.bat release

:: 2. 拷贝 Release 运行所需 DLL
cd build\release\bin
windeployqt LWVision.exe
```

确保 exe 同目录有以下 DLL（windeployqt 会自动拷大部分）：

- `Qt6Core.dll` `Qt6Gui.dll` `Qt6Widgets.dll` `Qt6Network.dll` `Qt6SerialPort.dll`
- `libgcc_s_seh-1.dll` `libstdc++-6.dll` `libwinpthread-1.dll`（MinGW 运行时）
- `libopencv_core500.dll` `libopencv_imgproc500.dll` 等（从 thirdparty\opencv\build\bin 拷）
- `DVPCamera64.dll` `dvpir64.dll`（装了 DVP2 SDK 才有）

## 六、常见问题

**Q1：CMake 报找不到 Qt6**
未设置 `QT_ROOT_DIR`。确认它指向 `...\6.x.x\mingw_64`（bin 里有 qmake.exe），或直接用 build.bat。

**Q2：`undefined reference to cv::xxx`**
用了 MSVC 版 OpenCV。改用项目自带的 MinGW 版（默认行为），或按第三节重新编译。

**Q3：编译中文乱码/转义警告**
CMake 已设 UTF-8 编码选项；确保源码文件以 UTF-8（无 BOM 亦可）保存。

**Q4：exe 能编译但启动闪退**
通常是缺 DLL 或 Qt 插件（platforms/qwindows.dll）。用 windeployqt 部署，或先在 Qt Creator 里运行确认。

**Q5：在 exFAT 移动硬盘上开发要注意什么**
git 功能正常；但建议重要节点 push 到远端仓库（GitHub/Gitee 私有仓库）做异地备份，移动硬盘不做唯一副本。
