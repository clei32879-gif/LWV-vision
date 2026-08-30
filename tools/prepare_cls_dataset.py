# -*- coding: utf-8 -*-
"""
prepare_cls_dataset.py — 把真实缺陷分类图组织成 ultralytics 分类训练格式
用法: python prepare_cls_dataset.py <源目录> <输出目录> [验证集比例=0.2]

源目录结构: <源>/<类别名>/*.bmp|png|jpg  (如 G:/图片素材/真实缺陷分类图-长/开裂/*.bmp)
输出结构:   <输出>/train/<类别名>/*.jpg
            <输出>/val/<类别名>/*.jpg    (按文件名哈希确定性划分)
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


def main():
    src_dir = Path(sys.argv[1]) if len(sys.argv) > 1 else Path("G:/图片素材/真实缺陷分类图-长")
    out_dir = Path(sys.argv[2]) if len(sys.argv) > 2 else Path("G:/guangxuan/testdata/cls_dataset")
    val_ratio = float(sys.argv[3]) if len(sys.argv) > 3 else 0.2

    if not src_dir.exists():
        print(f"[错误] 源目录不存在: {src_dir}")
        return 1

    classes = sorted([d.name for d in src_dir.iterdir() if d.is_dir()])
    # 过滤系统垃圾目录
    classes = [c for c in classes if not c.startswith(".")]
    print(f"类别({len(classes)}): {classes}")

    total_train = total_val = 0
    for cls in classes:
        files = [f for f in (src_dir / cls).iterdir()
                 if f.is_file() and f.suffix.lower() in VALID_EXT and not f.name.startswith("._")]
        for f in files:
            split = "val" if split_key(f) < int(val_ratio * 100) else "train"
            dst_dir = out_dir / split / cls
            dst_dir.mkdir(parents=True, exist_ok=True)
            dst = dst_dir / (f.stem + ".jpg")
            # BMP转JPG(压缩体积, 加速训练IO); 其他格式直接复制
            if f.suffix.lower() == ".bmp":
                # 中文路径兼容: fromfile+imdecode (imread在Windows不支持非ASCII路径)
                try:
                    data = np.fromfile(str(f), dtype=np.uint8)
                    img = cv2.imdecode(data, cv2.IMREAD_COLOR)
                except OSError:
                    img = None
                if img is None:
                    print(f"  [跳过] 读取失败: {f.name}")
                    continue
                # 中文路径安全写入: imencode + tofile (imwrite不支持非ASCII路径)
                ok, buf = cv2.imencode(".jpg", img, [cv2.IMWRITE_JPEG_QUALITY, 92])
                if ok:
                    buf.tofile(str(dst))
                else:
                    print(f"  [跳过] 编码失败: {f.name}")
                    continue
            else:
                shutil.copy2(f, dst)
            if split == "train":
                total_train += 1
            else:
                total_val += 1
        n_cls = len(list((out_dir / "train" / cls).glob("*.jpg"))) if (out_dir / "train" / cls).exists() else 0
        print(f"  {cls}: 训练{n_cls}张")

    print(f"完成: 训练{total_train}张, 验证{total_val}张 -> {out_dir}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
