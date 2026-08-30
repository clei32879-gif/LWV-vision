# -*- coding: utf-8 -*-
"""
train_cls.py — 教导向导的训练后端 (由软件通过QProcess调用)
用法: python train_cls.py <数据目录> <输出onnx路径> [轮数=60] [设备=cpu]

数据目录结构(教导向导已生成):
  <数据目录>/train/良品/*.jpg   <数据目录>/train/缺陷/*.jpg
  <数据目录>/val/良品/*.jpg     <数据目录>/val/缺陷/*.jpg
"""
import sys
import glob
import os
import shutil
from pathlib import Path


def main():
    data_dir = Path(sys.argv[1]) if len(sys.argv) > 1 else Path("teach_data")
    out_onnx = Path(sys.argv[2]) if len(sys.argv) > 2 else Path("teach_model.onnx")
    epochs = int(sys.argv[3]) if len(sys.argv) > 3 else 60
    device = sys.argv[4] if len(sys.argv) > 4 else "cpu"

    n_train = len(glob.glob(str(data_dir / "train" / "*" / "*")))
    n_val = len(glob.glob(str(data_dir / "val" / "*" / "*")))
    print(f"[教导训练] 数据: 训练{n_train}张 验证{n_val}张 轮数:{epochs} 设备:{device}")
    if n_train < 6:
        print("[教导训练] 样本太少(每类至少3张), 终止")
        return 2

    from ultralytics import YOLO

    m = YOLO("yolov8n-cls.pt")
    m.train(data=str(data_dir), epochs=epochs, imgsz=224, batch=8 if device == "cpu" else 16,
            workers=2, device=device, patience=30, optimizer="AdamW", lr0=0.001,
            seed=42, verbose=True)

    import glob as g
    bests = sorted(g.glob("runs/classify/train*/weights/best.pt"), key=os.path.getmtime)
    if not bests:
        print("[教导训练] 未找到best.pt")
        return 1
    best = bests[-1]
    print(f"[教导训练] 最佳权重: {best}")

    YOLO(best).export(format="onnx", imgsz=224)
    onnx_file = best.replace(".pt", ".onnx")
    out_onnx.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy(onnx_file, out_onnx)
    # 类别表随行
    src_cls = data_dir / "classes.txt"
    if src_cls.exists():
        shutil.copy(src_cls, out_onnx.parent / (out_onnx.stem + "_classes.txt"))
    print(f"[教导训练] 完成: {out_onnx}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
