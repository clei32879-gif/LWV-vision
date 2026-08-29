# LW Vision 立维视觉 — Windows 编译部署指南

## 一、环境准备

### 1.1 必装软件

| 软件 | 版本 | 下载地址 | 说明 |
|------|------|---------|------|
| **Qt** | 6.11.1 | https://www.qt.io/download-qt-installer | 选MinGW 13.1.0组件 |
| **CMake** | 3.21+ | https://cmake.org/download/ | 安装时选"Add to PATH" |
| **OpenCV** | 5.0.0 | https://opencv.org/releases/ | 解压到D:\Development\OpenCV |
| **Git** | 最新 | https://git-scm.com/download/win | 可选，用于版本管理 |

### 1.2 可选软件（相机SDK）

| SDK | 用途 | 安装路径 |
|-----|------|---------|
| DVP2 SDK | 度申相机 | C:\Program Files (x86)\DVP2 SDK CN |
| MvCameraControl | 海康相机 | 默认路径即可 |
| Pylon SDK | 巴斯勒相机 | 默认路径即可 |
| GxIAPI SDK | 大恒相机 | 默认路径即可 |

### 1.3 可选软件（AI推理）

| 软件 | 版本 | 用途 |
|------|------|------|
| ONNX Runtime | 1.17+ | YOLOv8推理，下载Windows x64版本 |

---

## 二、Qt安装步骤

### 2.1 下载安装Qt

1. 运行Qt在线安装程序
2. 登录/注册Qt账号
3. 选择组件：
   - ✅ Qt 6.11.1 → MinGW 13.1.0 64-bit
   - ✅ Developer and Designer Tools → MinGW 13.1.0
4. 安装到 `D:\Development\Qt`

### 2.2 配置环境变量

在系统环境变量PATH中添加：
```
D:\Development\Qt\6.11.1\mingw_64\bin
D:\Development\Qt\Tools\mingw1310_64\bin
D:\Development\Qt\Tools\mingw1310_64\opt\bin
```

验证：打开CMD输入
```
qmake --version
g++ --version
cmake --version
```

---

## 三、OpenCV安装步骤

### 3.1 下载OpenCV

1. 下载 OpenCV 5.0.0 Windows版
2. 运行安装程序，解压到 `D:\Development\OpenCV`
3. 最终路径：`D:\Development\OpenCV\opencv\build`

### 3.2 MinGW编译OpenCV（重要！）

> ⚠️ **MSVC版OpenCV和MinGW不兼容，必须用MinGW重新编译OpenCV**

```bat
cd D:\Development\OpenCV\opencv
mkdir build_mingw
cd build_mingw
cmake .. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=ON -DBUILD_TESTS=OFF -DBUILD_PERF_TESTS=OFF -DBUILD_EXAMPLES=OFF -DBUILD_opencv_python2=OFF -DBUILD_opencv_python3=OFF
mingw32-make -j8
mingw32-make install
```

编译完成后，OpenCV在：`D:\Development\OpenCV\opencv\build_mingw\install`

---

## 四、编译LW Vision

### 4.1 命令行编译（推荐）

```bat
:: 1. 打开CMD，进入项目目录
cd F:\guangxuan

:: 2. 创建build目录
mkdir build
cd build

:: 3. CMake配置（MinGW）
cmake .. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release -DOpenCV_DIR=D:\Development\OpenCV\opencv\build_mingw\install

:: 4. 编译
mingw32-make -j8

:: 5. 运行
bin\LWVision.exe
```

### 4.2 Qt Creator编译（图形界面，更简单）

1. 打开 **Qt Creator**
2. 菜单 → 文件 → 打开文件或项目 → 选择 `F:\guangxuan\CMakeLists.txt`
3. 配置项目：
   - 构建套件选择：**Qt 6.11.1 MinGW 64-bit**
   - 构建目录：`F:\guangxuan\build`
4. 点击左下角 **🔨构建** 按钮（或Ctrl+B）
5. 点击 **▶运行** 按钮（或Ctrl+R）

### 4.3 MSVC编译（如果用Visual Studio）

```bat
:: 打开 "x64 Native Tools Command Prompt for VS 2022"
cd F:\guangxuan
mkdir build_msvc
cd build_msvc
cmake .. -G "Visual Studio 17 2022" -A x64 -DOpenCV_DIR=D:\Development\OpenCV\opencv\build
cmake --build . --config Release -j8
```

---

## 五、编译选项说明

| 选项 | 默认值 | 说明 |
|------|--------|------|
| `-DVI_BUILD_PLUGINS=ON` | ON | 编译所有插件 |
| `-DVI_BUILD_TESTS=OFF` | OFF | 编译测试程序 |
| `-DOpenCV_DIR=xxx` | 自动检测 | OpenCV路径 |
| `-DCMAKE_BUILD_TYPE=Release` | Release | Release优化，Debug调试 |

---

## 六、编译常见问题

### Q1: OpenCV找不到
```
CMake Error: Could not find OpenCV
```
**解决：** 指定OpenCV路径：
```
cmake .. -DOpenCV_DIR=D:\Development\OpenCV\opencv\build_mingw\install
```

### Q2: MinGW和MSVC的OpenCV不兼容
```
undefined reference to cv::xxx
```
**解决：** 必须用MinGW编译OpenCV（见第三节），不能用MSVC预编译版。

### Q3: Qt版本不对
```
Could not find a package configuration file provided by "Qt6"
```
**解决：** 确保PATH中有Qt的cmake路径：
```
set CMAKE_PREFIX_PATH=D:\Development\Qt\6.11.1\mingw_64
```

### Q4: DVP2 SDK找不到
```
DVP2 SDK not found at ...
```
**解决：** 如果没有度申相机SDK，可以忽略（不影响编译，度申驱动会跳过）。如有SDK但路径不对，修改CMakeLists.txt中的DVP2_SDK_DIR。

### Q5: 编译中文乱码
```
warning: unknown escape sequence
```
**解决：** CMakeLists.txt已设置 `/utf-8`，如仍有问题确保源文件保存为UTF-8编码。

---

## 七、部署到工控机

### 7.1 打包

```bat
:: 在build目录下
windeployqt bin\LWVision.exe
```

### 7.2 所需DLL

确保以下DLL在exe同目录：
- Qt6Core.dll, Qt6Gui.dll, Qt6Widgets.dll, Qt6Xml.dll
- libgcc_s_seh-1.dll, libstdc++-6.dll, libwinpthread-1.dll (MinGW运行时)
- opencv_core500.dll, opencv_imgproc500.dll 等
- DVPCamera64.dll, dvpir64.dll (度申相机，如有)
- onnxruntime.dll (YOLOv8推理，如有)

### 7.3 目录结构

```
LWVision/
├── LWVision.exe          # 主程序
├── *.dll                 # 所有依赖DLL
├── plugins/              # 插件DLL（自动生成）
├── config/               # 配置文件
│   └── global.json       # 全局配置
└── projects/             # 项目文件
    └── <项目名>/
        └── project.json  # 项目配置
```

---

## 八、快速开始（5分钟上手）

1. **安装Qt** → 选MinGW 13.1.0
2. **安装CMake** → 勾选Add to PATH
3. **编译OpenCV** → MinGW编译一次（约15分钟）
4. **打开Qt Creator** → 打开CMakeLists.txt → 构建 → 运行
5. 完成 ✅
