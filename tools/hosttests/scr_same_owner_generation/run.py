import hashlib
from pathlib import Path
import subprocess
import sys
import tempfile

if len(sys.argv) != 2:
    sys.exit("usage: python3 run.py /path/to/fixed-nuttx")
root = Path(sys.argv[1]).resolve()
package = Path(__file__).resolve().parent
fixture = Path("tools/hosttests/scr_containment")
lower = Path("arch/arm/src/stm32h7/stm32_sdmmc.c")
header = Path("include/nuttx/scr_lease_review.h")
pins = {
    fixture / "snapshot.c": "fc6e7b66bd35c39f40f3655a0ad6eca5f71693d5",
    fixture / "actual-command-constants.h": "301f576caf9410da7bd5c5258b3d737358402ed3",
    lower: "affa479b1cfebd404dd880d7ffa19466a07457ba",
    header: "165fc50c187891041ce3aa8241bf27be72cbadc0",
}
src = {}
for path, expected in pins.items():
    data = (root / path).read_bytes()
    sha = hashlib.sha1(f"blob {len(data)}\0".encode() + data).hexdigest()
    if sha != expected:
        sys.exit(f"mismatch {path}: {sha} != {expected}")
    print(f"PIN {sha} {path}", flush=True)
    src[path] = data.decode()

def replace_once(body, old, new):
    if body.count(old) != 1:
        sys.exit(f"unexpected fixture boundary: {old!r}")
    return body.replace(old, new, 1)

drv = src[lower].splitlines(keepends=True)
def lines(a, b):
    return "".join(drv[a-1:b])

snap = src[fixture / "snapshot.c"]
start = "int stm32_scr_claim("
end = "static void stm32_datadisable("
old = snap[snap.index(start):snap.index(end)]
# Reproduce the active SCR branch around the exact lower-driver projection.
claim = lines(987, 1029)
tail = lines(1065, 1389)
enabled = "#ifdef CONFIG_SD_SCR_CONTAINMENT_REVIEW\n"
patched = replace_once(snap, old, enabled + claim + "\n" + tail + "\n")
marker = "static int send_hook(struct sdio_dev_s*d,uint32_t cmd,uint32_t arg){"
patched = replace_once(patched, marker, (package / "check.c.in").read_text() + "\n" +
    marker + "if(check_previous && (cmd&MMCSD_CMDIDX_MASK)==55) old_key_at_cmd55((void*)d);")
patched = replace_once(patched, "memset((void*)regs[8],0x5a,8);",
                       "memset((void*)regs[8],check_previous?0xa6:0x5a,8);")
main = "int main(void){"
if patched.count(main) != 1:
    sys.exit("fixture main boundary not unique")
patched = patched.split(main)[0] + (package / "scenario.c.in").read_text()

with tempfile.TemporaryDirectory(prefix="scr-stale-key-") as d:
    build = Path(d)
    c, elf = build / "test.c", build / "test"
    c.write_text(patched)
    (build / "nuttx").mkdir()
    (build / "nuttx/config.h").write_text("/* Empty host config: sector and target features are off. */\n")
    for opt in ("-O0", "-O2"):
        print(opt, flush=True)
        subprocess.run(["gcc", "-std=c11", opt, "-DCONFIG_STM32H7_SDMMC_IDMA",
                        "-fsanitize=undefined", "-fno-sanitize-recover=all",
                        "-I" + str(build), "-I" + str(root / fixture), "-I" + str(root / header.parent),
                        str(c), "-o", str(elf)], check=True)
        subprocess.run([str(elf)], check=True)
