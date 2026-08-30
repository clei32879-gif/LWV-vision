# testdata — 标准测试图片库

> 本目录的图片由 `tools/testdata_gen.cpp` 确定性生成（不含随机数，可完全复现），不入 git。
> 每个阶段做算法改动后，用本图库回归验证，结果记录到各阶段文档。

## 内容

| 文件 | 说明 |
|------|------|
| `OK_01..24.png` | 完整外螺纹工件（相位旋转+位置微移，模拟转盘过片） |
| `NG_MISSING_XX.png` | 缺牙（牙型出现缺口） |
| `NG_BURR_XX.png` | 毛刺（牙顶亮刺） |
| `NG_DAMAGED_XX.png` | 烂牙（局部牙型抖动坍塌） |
| `NG_SLANT_XX.png` | 斜牙（牙线倾斜14°） |
| `calib_board/board_front.png` | 棋盘格标定板（正交，内角点9×6，单格60px） |
| `calib_board/board_rot12.png` | 棋盘格标定板（绕图心旋转12°，验证带角度检测） |

> `calib_board/` 供 标定校准-棋盘格标定 回归：板单格实长为 10mm → 理论比例 10/60≈0.1667 mm/px。

## 重新生成

```bat
:: 生成到 build 输出目录（虚拟相机默认读取 build\release\bin\testdata\virtual_camera）
:: 注意: 命令行直接跑 testdata_gen 需先把 OpenCV/Qt DLL 目录加入 PATH, 否则加载 DLL 失败崩溃
cmake --build build/release --target generate_testdata

:: 或手动指定目录与数量
testdata_gen [输出目录] [OK张数] [每类NG张数]
```

## 真实实物图

`real_samples/` — 度申相机实拍精选样本（螺丝缺陷分类/螺母多视角），原始图库见 `G:\图片素材`（1.3GB不入库）。
详情看 [real_samples/README.md](real_samples/README.md)。

## 用途

1. **虚拟相机回放**：`testdata/virtual_camera/` 下的图会被虚拟相机按帧率循环播放
2. **算法回归**：阶段2/3 的检测工具用本图库逐个验证（找圆→牙数统计→缺牙检测）
3. **演示**：无硬件时向客户演示完整检测流程
