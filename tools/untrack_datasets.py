# -*- coding: utf-8 -*-
"""一次性清理: 把数据集文件从git索引移除(保留磁盘文件)"""
import subprocess
import sys

r = subprocess.run(['git', 'ls-files', '-z'], capture_output=True)
files = [f.decode('utf-8', 'replace') for f in r.stdout.split(b'\0')
         if ('cls_dataset' in f.decode('utf-8', 'replace') or 'mvtec' in f.decode('utf-8', 'replace'))]
print(f'待解除跟踪: {len(files)}')

# 批量: xargs式, 每批50个
for i in range(0, len(files), 50):
    batch = files[i:i + 50]
    r = subprocess.run(['git', 'rm', '--cached', '-q', '--'] + batch,
                       capture_output=True, text=True)
    if r.returncode != 0:
        print('批失败:', r.stderr[:100])
        # 逐个
        for f in batch:
            subprocess.run(['git', 'rm', '--cached', '-q', '--', f], capture_output=True)

r2 = subprocess.run(['git', 'ls-files'], capture_output=True, text=True, encoding='utf-8')
left = [f for f in r2.stdout.splitlines() if ('cls_dataset' in f or 'mvtec' in f)]
print(f'剩余跟踪: {len(left)}')
sys.exit(0 if not left else 1)
