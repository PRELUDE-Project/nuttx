"""Verify a misplaced directive at the end of the fixed SCR-only driver block."""
import hashlib
from pathlib import Path
import subprocess
import sys

if len(sys.argv) != 2:
    sys.exit("usage: python3 check_guard.py /path/to/fixed-nuttx")
path = Path(sys.argv[1]) / "arch/arm/src/stm32h7/stm32_sdmmc.c"
data = path.read_bytes()
sha = hashlib.sha1(f"blob {len(data)}\0".encode() + data).hexdigest()
if sha != "affa479b1cfebd404dd880d7ffa19466a07457ba":
    sys.exit(f"unexpected H7 driver {sha}")
lines = data.decode().splitlines(keepends=True)
guard = lines[907]
tail = lines[1374:1389]
if guard != "#ifdef CONFIG_SD_SCR_CONTAINMENT_REVIEW\n" or tail[-2:] != ["#endif\n", "}\n"]:
    sys.exit("unexpected guard boundary")
fixed = tail[:-2] + [tail[-1], tail[-2]]

# この区間の外はホストの型宣言に置き換え、元の関数と境界の順序は保持します。
preamble = r"""
#include <stdbool.h>
#include <stdint.h>
#include <errno.h>
struct sdio_dev_s { int unused; };
struct scr_lease_review { int unused; };
struct scr_lease_key { uintptr_t owner; };
struct stm32_dev_s { struct scr_lease_review scrlease; };
typedef unsigned irqstate_t;
irqstate_t enter_critical_section(void);
void leave_critical_section(irqstate_t);
bool scr_lease_matches(struct scr_lease_review *, struct scr_lease_key);
bool up_interrupt_context(void);
bool scr_closed(struct stm32_dev_s *);
int nxsched_gettid(void);
"""
for name, section in (("baseline", tail), ("candidate", fixed)):
    for enabled in (False, True):
        command = ["gcc", "-x", "c", "-std=c11", "-fsyntax-only", "-"]
        if enabled:
            command.append("-DCONFIG_SD_SCR_CONTAINMENT_REVIEW")
        result = subprocess.run(command, input=preamble + guard + "".join(section),
                                text=True, capture_output=True)
        expected_failure = name == "baseline" and not enabled
        if expected_failure:
            if result.returncode == 0 or "before '}' token" not in result.stderr:
                sys.exit(result.stderr or "baseline unexpectedly accepted")
            print(f"{name} SCR OFF: rejected, out-of-function closing brace")
            print(result.stderr.rstrip())
        else:
            if result.returncode != 0:
                sys.exit(result.stderr)
            print(f"{name} SCR {'ON' if enabled else 'OFF'}: syntax accepted")
