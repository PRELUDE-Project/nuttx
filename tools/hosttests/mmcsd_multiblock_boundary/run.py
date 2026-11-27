#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""固定版の複数ブロック書込みを、実機なしで検証する。"""
import argparse
import hashlib
from pathlib import Path
import re
import shutil
import subprocess
from tempfile import TemporaryDirectory

HERE = Path(__file__).resolve().parent
DRIVER = Path("drivers/mmcsd/mmcsd_sdio.c")
SOURCE_SHA256 = "921af2bcedce55624ab550a60bf70a4b8c4e7db6fd1f3dbf5cc80a81162df684"


def extract_write(source):
    # 宣言でなく定義を選び、コメントや文字列中の波括弧は数えない。
    pattern = r"(?m)^static ssize_t mmcsd_writemultiple\([^;{}]*\)\s*\{"
    entries = list(re.finditer(pattern, source))
    if len(entries) != 1:
        raise SystemExit("mmcsd_writemultiple の定義を1件に絞れません")
    start = entries[0]
    nesting = 1
    tokens = r'/\*[\s\S]*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"|[{}]'
    for match in re.finditer(tokens, source[start.end():]):
        nesting += (match[0] == "{") - (match[0] == "}")
        if nesting == 0:
            return source[start.start():start.end() + match.end()]
    raise SystemExit("mmcsd_writemultiple の終端が見つかりません")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo", type=Path, default=HERE.parents[2])
    opt = parser.parse_args()
    data = (opt.repo / DRIVER).read_bytes()
    if hashlib.sha256(data).hexdigest() != SOURCE_SHA256:
        raise SystemExit(f"固定版と異なるため実行しません: {DRIVER}")
    compiler = shutil.which("cc")
    if compiler is None:
        raise SystemExit("UBSanの使えるCコンパイラーが必要です")

    with TemporaryDirectory(prefix="mmcsd-cmd25-host-") as path:
        work = Path(path)
        (work / "actual.c.inc").write_text(extract_write(data.decode("utf-8")),
                                           encoding="utf-8")
        for dma in (False, True):
            for notice in (False, True):
                flags = []
                if dma:
                    flags.append("-DCONFIG_SDIO_DMA")
                if notice:
                    flags.append("-DCONFIG_MMCSD_SDIOWAIT_WRCOMPLETE")
                print(f"{dma=}, {notice=}", flush=True)
                subprocess.run(
                    [compiler, "-std=c11", "-Wall", "-Wextra", "-Werror",
                     "-Wno-unused-parameter", "-fsanitize=undefined",
                     "-fno-sanitize-recover=all", *flags, "-I", str(work),
                     str(HERE / "fixture.c"), "-o", str(work / "test")],
                    check=True)
                subprocess.run([str(work / "test")], check=True)


if __name__ == "__main__":
    main()
