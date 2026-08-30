# -*- coding: utf-8 -*-
"""
prepare_cls_dataset.py — 把真实缺陷分类图组织成 ultralytics 分类训练格式
用法: python prepare_cls_dataset.py <输出目录> <源目录1> [源目录2 ...] [验证集比例=0.2]

源目录结构: <源>/<类别名>/*.bmp|png|jpg  (支持多个源目录, 同名类别自动合并)
输出结构:   <输出>/train/<类别名>/*.jpg
            <输出>/val/<类别名>/*.jpg    (按文件名哈希确定性划分)
            <输出>/classes.txt           (类别表, 索引=排序顺序)
"""
import sys
import hashlib
import shutil
from pathlib import Path

import cv2
import numpy as np

VALID_EXT = {".bmp", ".png", ".jpg", ".jpeg"}


def split_key(path: Path) -> int:
    """文件名确定性哈希(0-99), 保证多次运行划分一致"""
    return int(hashlib.md5(path.name.encode("utf-8")).hexdigest(), 16) % 100


def convert_or_copy(src: Path, dst: Path) -> bool:
    """BMP转JPG(中文路径安全: imdecode+tofile), 其他格式直接复制"""
    if dst.exists():
        return True
    if src.suffix.lower() == ".bmp":
        try:
            data = np.fromfile(str(src), dtype=np.uint8)
            img = cv2.imdecode(data, cv2.IMREAD_COLOR)
        except OSError:
            img = None
        if img is None:
            print(f"  [跳过] 读取失败: {src.name}")
            return False
        ok, buf = cv2.imencode(".jpg", img, [cv2.IMWRITE_JPEG_QUALITY, 92])
        if not ok:
            print(f"  [跳过] 编码失败: {src.name}")
            return False
        buf.tofile(str(dst))
        return True
    shutil.copy2(src, dst)
    return True


def main():
    # 用法: prepare_cls_dataset.py <输出目录> <源目录1> [源目录2 ...] [验证比例]
    args = sys.argv[1:]
    out_dir = Path(args[0]) if args else Path("D:/LWVision/testdata/cls_dataset")
    val_ratio = 0.2
    srcs = []
    for a in args[1:]:
        try:
            v = float(a)
            if 0 < v < 1:
                val_ratio = v
                continue
        except ValueError:
            pass
        srcs.append(Path(a))
    if not srcs:
        srcs = [Path("G:/图片素材/真实缺陷分类图-长"),
                Path("G:/图片素材/真实缺陷分类图-短")]

    for s in srcs:
        if not s.exists():
            print(f"[错误] 源目录不存在: {s}")
            return 1

    # 合并所有源目录的类别 (同名类别文件合并)
    classes = set()
    for s in srcs:
        classes |= {d.name for d in s.iterdir()
                    if d.is_dir() and not d.name.startswith(".")}
    classes = sorted(classes)
    print(f"类别({len(classes)}): {classes}")

    total_train = total_val = 0
    for cls in classes:
        for s in srcs:
            cls_dir = s / cls
            if not cls_dir.exists():
                continue
            for f in cls_dir.iterdir():
                if not f.is_file() or f.suffix.lower() not in VALID_EXT:
                    continue
                if f.name.startswith("._"):
                    continue
                split = "val" if split_key(f) < int(val_ratio * 100) else "train"
                dst_dir = out_dir / split / cls
                dst_dir.mkdir(parents=True, exist_ok=True)
                dst = dst_dir / (f.stem + ".jpg")
                if convert_or_copy(f, dst):
                    if split == "train":
                        total_train += 1
                    else:
                        total_val += 1

    # 类别表 (索引=按名称排序, 与ultralytics分类的类别分配一致)
    (out_dir / "classes.txt").write_text("\n".join(classes) + "\n", encoding="utf-8")
    print(f"完成: 训练{total_train}张, 验证{total_val}张 -> {out_dir}")
    print(f"类别表: {out_dir / 'classes.txt'}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
