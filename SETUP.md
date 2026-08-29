# LW Vision 立维视觉 - 项目设置指南

## 系统要求

### 编译器
- **Qt 6.11.1** + MinGW 13.1.0
- **或** Visual Studio 2022 Build Tools (C++ workload)

### 第三方库
- **OpenCV 5.0.0** (MSVC版本，已安装在 `D:\Development\OpenCV\opencv\build`)
- **DVP2 SDK** (度申相机SDK，已安装在 `C:\Program Files (x86)\DVP2 SDK CN`)

## 项目结构

```
E:\guangxuan\
├── CMakeLists.txt          # 根CMake配置
├── src/                    # 源代码
│   ├── app/               # 主程序
│   ├── core/              # 核心模块
│   ├── engine/            # 流程引擎
│   ├── hal/               # 硬件抽象层
│   ├── ui/                # 用户界面
│   └── utils/             # 工具类
├── plugins/               # 插件
│   ├── cameras/           # 相机驱动
│   ├── tools/             # 视觉工具
│   └── logic/             # 逻辑工具
├── thirdparty/            # 第三方库
│   └── opencv/           # OpenCV源码
├── build/                 # 构建目录
└── learning/              # 学习资料
```

## 在另一台电脑上配置

### 步骤1：安装Qt
```bash
# 下载Qt在线安装程序
# https://www.qt.io/download-qt-installer
# 安装Qt 6.11.1 + MinGW 13.1.0
```

### 步骤2：安装OpenCV
```bash
# 下载OpenCV 5.0.0 MSVC版本
# https://opencv.org/releases/
# 安装到 D:\Development\OpenCV\opencv
```

### 步骤3：安装DVP2 SDK（如需要）
```bash
# 从项目 thirdparty 目录安装
# 或从度申官网下载
```

### 步骤4：配置环境变量
```bash
# 添加到PATH:
# D:\Development\Qt\6.11.1\mingw_64\bin
# D:\Development\Qt\Tools\mingw1310_64\bin
```

### 步骤5：构建项目
```bash
# 打开Qt Creator
# 打开 E:\guangxuan\CMakeLists.txt
# 配置构建目录
# 点击构建
```

## 已实现的功能

### 视觉工具 (27个)
- 图像处理: 采集图像、图像滤波、阈值分割、形态学、颜色转换
- 检测识别: Blob分析、亮度检查、灰度匹配、形状匹配、轮廓匹配、边缘检测、线检测、圆检测、图像对比
- 几何测量: 距离测量、角度测量、卡尺
- 识别工具: 条码识别、二维码识别、字符识别
- 逻辑控制: 计算器、数据判断、条件分支、循环、延时
- 特殊工具: 数据显示、更新视图、标定校准

### UI功能
- 深色主题界面
- 工具箱面板
- 流程编辑器 (拖拽、右键菜单、属性编辑)
- 状态面板 (OK/NG指示器)
- 图像缩放控制 (Zoom In/Out/1:1/Fit)
- 文件选择对话框

## 已知问题

### OpenCV兼容性
- MSVC版本OpenCV与MinGW编译器ABI不兼容
- 解决方案: 使用MSVC编译器，或用MinGW从源码编译OpenCV

### 待实现功能
- 完整的视觉检测流水线
- 相机参数配置界面
- 项目保存/加载
- 多视图布局
