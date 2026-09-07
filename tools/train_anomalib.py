#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
anomalib 无监督缺陷检测训练脚本 (LWVision 阶段4: 纯OK图即可训练)
用法:
    python tools/train_anomalib.py                     # 默认 Padim + 螺纹OK图
    python tools/train_anomalib.py --model Padim --data testdata/virtual_camera
产物:
    models/anomalib_thread/padim.onnx     — ONNX 模型 (C++ 推理)
    models/anomalib_thread/metadata.json  — 归一化参数 (C++ 端复刻预处理必需)
"""
import argparse
import json
import os
import sys
from pathlib import Path

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--model", default="Padim", choices=["Padim", "Patchcore", "Fastflow", "EfficientAd"])
    ap.add_argument("--data", default="testdata/virtual_camera")
    ap.add_argument("--out", default="models/anomalib_thread")
    ap.add_argument("--size", type=int, default=256)
    ap.add_argument("--backbone", default="resnet18")
    ap.add_argument("--align", action="store_true",
                    help="找圆对齐: 以最大内圆为中心裁剪 roiSize 方形区域")
    args = ap.parse_args()

    try:
        from anomalib.data import Folder
        from anomalib.models import Padim, Patchcore, Fastflow, EfficientAd
        from anomalib.engine import Engine
        from anomalib.deploy import ExportType
    except ImportError as e:
        print(f"[错误] anomalib 未安装: {e}\n  pip install anomalib")
        return 1

    model_cls = {"Padim": Padim, "Patchcore": Patchcore,
                 "Fastflow": Fastflow, "EfficientAd": EfficientAd}[args.model]

    # 数据: 全部当"正常"训练 (无监督), 临时目录只放 OK 图
    # 可选对齐: --align 圆心定位+以螺纹环带为中心裁剪 (OK图工件位置随机时必需 —
    #   无监督模型对位置偏移敏感, 这也是竞品"先定位矫正再判"方法论的原因)
    data_root = Path(args.data)
    tmp_ok = Path("testdata/anomalib_ok")
    tmp_ok.mkdir(parents=True, exist_ok=True)
    import shutil, cv2
    import numpy as np
    count = 0
    for p in sorted(data_root.glob("OK*.png")):
        if args.align:
            im = cv2.imread(str(p))
            g = cv2.cvtColor(im, cv2.COLOR_BGR2GRAY)
            # Hough 找主圆 (与 C++ 快速找圆的 Hough 回退同思路)
            circles = cv2.HoughCircles(g, cv2.HOUGH_GRADIENT, 1.2, 200,
                                       param1=120, param2=40, minRadius=40, maxRadius=0)
            if circles is not None and len(circles[0]):
                cx, cy, r = circles[0][0]
                half = int(r * 1.05) + 4   # 收紧: 只留螺纹环带, 减少背景
                x0, y0 = int(cx - half), int(cy - half)
                s2 = max(half * 2, 32)
                H, W = im.shape[:2]
                canvas = np.zeros((s2, s2, 3), np.uint8)
                sx0, sy0 = max(0, -x0), max(0, -y0)
                dx0, dy0 = max(0, x0), max(0, y0)
                cw, ch = min(s2 - sx0, W - dx0), min(s2 - sy0, H - dy0)
                if cw > 16 and ch > 16:
                    canvas[sy0:sy0+ch, sx0:sx0+cw] = im[dy0:dy0+ch, dx0:dx0+cw]
                    im = canvas
            im = cv2.resize(im, (args.size, args.size))
        dst = tmp_ok / p.name
        cv2.imwrite(str(dst), im)
        count += 1
    print(f"[数据] OK 图 {count} 张 → {tmp_ok} (align={args.align})")

    from torchvision.transforms.v2 import Resize, Compose
    augmentations = Compose([Resize((args.size, args.size), antialias=True)])
    batch_size = 1 if args.model == "EfficientAd" else 32  # EfficientAd 强制 batch=1
    datamodule = Folder(
        name="thread_ok",
        root=str(tmp_ok.parent),
        normal_dir=tmp_ok.name,
        augmentations=augmentations,
        val_split_mode="synthetic",  # 从 normal 合成验证集(不抽走训练图), bank 见全部OK图
        train_batch_size=batch_size,
    )

    print(f"[训练] {args.model} @ {args.size}x{args.size} backbone={args.backbone} batch={batch_size}")
    import torch
    engine = Engine()
    try:
        model = model_cls(backbone=args.backbone)
    except TypeError:
        model = model_cls()  # EfficientAd 等无 backbone 参数
    engine.fit(model=model, datamodule=datamodule)

    out_dir = Path(args.out)
    out_dir.mkdir(parents=True, exist_ok=True)
    print(f"[导出] ONNX → {out_dir} (无后处理: 原始异常分数)")
    # 关键: anomalib 2.x 导出会把 post_processor(normalize+clip)固化进 ONNX → OK/NG 全变1.0
    # 解法: 临时摘掉 post_processor 再导出, ONNX 输出 = 原始 (pred_score, anomaly_map)
    saved_pp = getattr(model, "post_processor", None)
    if saved_pp is not None:
        model.post_processor = torch.nn.Identity()
    try:
        engine.export(model=model, export_type=ExportType.ONNX, export_root=str(out_dir))
    finally:
        if saved_pp is not None:
            model.post_processor = saved_pp

    # 导出归一化元数据 (C++ 端复刻预处理/后处理必需: 图像均值方差 + 异常阈值)
    meta = {
        "model": args.model,
        "image_size": [args.size, args.size],
        "imagenet_mean": [0.485, 0.456, 0.406],
        "imagenet_std": [0.229, 0.224, 0.225],
        "note": "阈值需用 OK/NG 各若干张跑推理后按 F1 最优选",
    }
    # 提取判定阈值: anomalib 2.x 在 lightning_module.post_processor._image_threshold
    try:
        lm = engine.trainer.lightning_module
        pp = getattr(lm, "post_processor", None)
        if pp is not None and hasattr(pp, "_image_threshold"):
            import torch as _t
            thr = pp._image_threshold
            meta["image_threshold"] = float(thr.item() if _t.is_tensor(thr) else thr)
        ppt = getattr(pp, "_pixel_threshold", None)
        if ppt is not None:
            import torch as _t
            meta["pixel_threshold"] = float(ppt.item() if _t.is_tensor(ppt) else ppt)
        if hasattr(pp, "enable_normalization"):
            meta["enable_normalization"] = bool(pp.enable_normalization)
        print(f"[阈值] image_threshold = {meta.get('image_threshold')}")
    except Exception as e:
        print(f"[提示] 元数据提取失败: {e}")

    with open(out_dir / "metadata.json", "w", encoding="utf-8") as f:
        json.dump(meta, f, ensure_ascii=False, indent=2)

    # 验证: OK 图分数应低, NG 图分数应高
    import torch, cv2, numpy as np
    import onnxruntime as ort
    onnx_path = out_dir / "model.onnx"
    if not onnx_path.exists():
        cands = list(out_dir.rglob("*.onnx"))
        onnx_path = cands[0] if cands else onnx_path
    if onnx_path.exists():
        sess = ort.InferenceSession(str(onnx_path), providers=["CPUExecutionProvider"])
        inp_name = sess.get_inputs()[0].name

        def infer(path):
            im = cv2.imread(str(path))
            im = cv2.cvtColor(im, cv2.COLOR_BGR2RGB)
            im = cv2.resize(im, (args.size, args.size)).astype(np.float32) / 255.0
            im = (im - np.array(meta["imagenet_mean"])) / np.array(meta["imagenet_std"])
            im = im.transpose(2, 0, 1)[None].astype(np.float32)
            out = sess.run(None, {inp_name: im})
            return float(out[0].flatten()[0])  # 原始 pred_score (无归一化)

        ok_scores = [infer(p) for p in sorted(tmp_ok.glob("OK*.png"))[:6]]
        ng_scores = [infer(p) for p in sorted(data_root.glob("NG_*.png"))[:6]]
        print(f"[验证] OK 分数: {[f'{s:.3f}' for s in ok_scores]}")
        print(f"[验证] NG 分数: {[f'{s:.3f}' for s in ng_scores]}")
        if ok_scores and ng_scores:
            sep = (min(ng_scores) - max(ok_scores))
            print(f"[验证] 可分性(NG最低-OK最高): {sep:+.3f} {'✓ 可分' if sep > 0 else '✗ 有重叠, 需调阈值/换模型'}")

    print(f"[完成] 模型: {onnx_path}")
    return 0

if __name__ == "__main__":
    sys.exit(main())
