# -*- coding: utf-8 -*-
"""给15个有applyCorrection但缺useCorrection属性的工具补上属性定义(跟随时生效的开关)"""
import re
from pathlib import Path

TOOLS = [
    "BlobAnalysis", "BlobClassify", "CircleDetection", "ColorDetection",
    "EdgeDetection", "EdgeSpacing", "Caliper", "CropTransform",
    "ImageOperation", "LineDetection", "MultiContourMatch",
    "PixelStatistics", "ScanEdge", "ThreadInspection", "VertexDetection",
]

PROP_LINE = '        PropertyDef::boolProp("useCorrection", "跟随位置补正", false, "ROI"),\n'

for t in TOOLS:
    cpp = Path(f"D:/LWVision/plugins/tools/{t}/{t}.cpp")
    s = cpp.read_text(encoding="utf-8")
    if '"useCorrection"' in s:
        print(f"  跳过(已有): {t}")
        continue
    # 找 propertyDefs 的 return { 行, 在其后插入属性
    m = re.search(r"(PropertyDefList \w+::propertyDefs\(\) const \{\s*return \{\n)", s)
    if not m:
        print(f"  [失败] 找不到propertyDefs: {t}")
        continue
    s = s[:m.end()] + PROP_LINE + s[m.end():]
    cpp.write_text(s, encoding="utf-8")
    print(f"  补齐: {t}")
