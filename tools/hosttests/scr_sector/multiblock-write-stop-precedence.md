# 通常SD書込み：CMD25データ転送とCMD12のエラー優先順位

固定ソースに対するホスト回帰です。普通のPIOで512 B×2セクタ、SDの通常CMD23は非対応、ACMD23は成功する条件です。

準備：PRELUDE-Project/nuttx の確認済みコミット `0f768fbb` の5入力ファイル（同梱せず）、Python 3、GCC（UBSan対応）。入力はファイルごとに既知のGit blobハッシュで照合します。転送実装の本体は固定ファイルから8関数を逐語抽出し、SDIO、ロックと転送readyをホスト用代役で制御します。

```sh
sh run.sh /path/to/pinned-nuttx-source
```

データタイムアウト＋CMD12 R1エラーは `-ETIMEDOUT`、データ完了＋CMD12 R1エラーは `-EIO`、双方成功は2を返します。3例ともロック保持中にCMD12を1回試行し、上位のCANCELは0回で、ロックを解除します。失敗時は `wrbusy` を立てず、書込み完了待ちも設定しません。書込み完了待ちの設定有無（両方）と `-O0/-O2` で確認しました。

R1応答を返す段階のSTOP失敗を扱います。実機、下位STM32ドライバ、DMA、実カードがCMD12で回復するか、データが実際に保存されたかは未検証です。下に固定版の実行ログを載せています。再現時には作業フォルダーへ `run.sh`、`project.py`、`fixture.c.in` の3つを保存します。

### run.sh

```sh
#!/bin/sh
set -eu
if [ "$#" -ne 1 ]; then
  printf 'Usage: %s /path/to/pinned-nuttx-source\n' "$0" >&2
  exit 2
fi
here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
tmp=$(mktemp -d)
trap 'rm -f "$tmp/fixture.c" "$tmp/test"; rmdir "$tmp"' EXIT
python3 "$here/project.py" "$1" "$here/fixture.c.in" "$tmp/fixture.c"
gcc --version | sed -n '1p'
for optimize in 0 2; do
  for wrcomplete in 0 1; do
    printf '%s\n' "-O$optimize WRCOMPLETE=$wrcomplete"
    set --
    if [ "$wrcomplete" -eq 1 ]; then
      set -- -DCONFIG_MMCSD_SDIOWAIT_WRCOMPLETE
    fi
    gcc "-O$optimize" -std=c11 -Wall -Wextra -Werror -fsanitize=undefined \
      -fno-sanitize-recover=undefined "$@" "$tmp/fixture.c" -o "$tmp/test"
    "$tmp/test"
  done
done
```

### project.py

```python
import hashlib
from pathlib import Path
import re
import sys

root, template, generated = map(Path, sys.argv[1:])
blobs = {
    "drivers/mmcsd/mmcsd_sdio.c": "cc659e003afd1ec1fd4fee3d7b1a0b807bdc4270",
    "include/nuttx/sdio.h": "23a2550ef0b0ec5dd542d0a24e478c6d0a24cd99",
    "drivers/mmcsd/mmcsd_sdio.h": "76dcf15c6c4922c97f7e5f3b1180b304506c1274",
    "drivers/mmcsd/mmcsd.h": "ff20eb283a56f61226116486fe733b30153ff55e",
    "drivers/mmcsd/Kconfig": "e550fdcbe648eadd8e358996eeccb15814ce035d",
}
inputs = {}
for path, expected in blobs.items():
    data = (root / path).read_bytes()
    got = hashlib.sha1(f"blob {len(data)}\0".encode() + data).hexdigest()
    if got != expected:
        sys.exit(f"{path}: expected Git blob {expected}, got {got}")
    print(f"{expected}  {path}")
    inputs[path] = data.decode("utf-8")
macro_names = """
SDIOWAIT_TRANSFERDONE SDIOWAIT_TIMEOUT SDIOWAIT_ERROR SDIOWAIT_WRCOMPLETE SDIO_CAPS_DMABEFOREWRITE
MMCSD_CMDIDX12 MMCSD_CMDIDX16 MMCSD_CMDIDX25 SD_CMDIDX55 SD_ACMDIDX23
MMCSD_RESPONSE_SHIFT MMCSD_R1_RESPONSE MMCSD_R1B_RESPONSE MMCSD_NODATAXFR MMCSD_DATAXFR_SHIFT
MMCSD_DATAXFR MMCSD_WRXFR MMCSD_WRDATAXFR MMCSD_MULTIBLOCK MMCSD_STOPXFR
MMCSD_CMD12 MMCSD_CMD16 MMCSD_CMD25 SD_CMD55 SD_ACMD23
MMCSD_R1_CARDISLOCKED MMCSD_R1_ERROR MMCSD_R1_ERRORMASK
MMC_CMD6_MODE MMC_CMD6_MODE_SHIFT MMC_CMD6_MODE_WRITE_BYTE MMC_CMD6_INDEX MMC_CMD6_INDEX_SHIFT
MMC_CMD6_VALUE MMC_CMD6_VALUE_SHIFT EXT_CSD_PART_CONF
IS_SD IS_BLOCK MMCSD_CARDTYPE_SDV1 MMCSD_CARDTYPE_SDV2 MMCSD_CARDTYPE_BLOCK
""".split()
header_lines = "\n".join(data for path, data in inputs.items() if path.endswith(".h"))
defines = []
for macro in macro_names:
    found = re.findall(rf"^#[ \t]*define[ \t]+{macro}\b[^\n]*", header_lines, re.M)
    if len(found) != 1:
        sys.exit(f"{macro}: expected 1 definition, found {len(found)}")
    defines.append(found[0].split("/*", 1)[0].rstrip())
entry = re.search(r"^config MMCSD_BLOCK_WDATADELAY\n(?:(?!^config )[\s\S])*?^[ \t]+default (\d+)$",
                  inputs["drivers/mmcsd/Kconfig"], re.M)
if not entry:
    sys.exit("Missing Kconfig write time default")
defines.append(f"#define PINNED_WDATADELAY {entry[1]}")
source = inputs["drivers/mmcsd/mmcsd_sdio.c"].splitlines()
ranges = [(336, 358), (413, 438), (1342, 1359), (1370, 1409), (1537, 1556),
          (1567, 1599), (2161, 2439), (2596, 2666)]
code = "\n\n".join("\n".join(source[a-1:b]) for a, b in ranges)
generated.write_text(template.read_text().replace("@PINNED_DEFINES@", "\n".join(defines))
                     .replace("@PINNED_SOURCE@", code))
```

### fixture.c.in

```c
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <errno.h>

@PINNED_DEFINES@
#define FAR
#define OK 0
#define debug_unused(...) ((void)0)
#define ferr(...) debug_unused(__VA_ARGS__)
#define finfo(...) debug_unused(__VA_ARGS__)
#define CHECK(expr) do { if (!(expr)) { fprintf(stderr, "check failed at %d: %s\n", __LINE__, #expr); exit(1); } } while (0)
#define DEBUGASSERT CHECK
#define MMCSD_MULTIBLOCK_LIMIT 2
#define MMCSD_BLOCK_WDATADELAY PINNED_WDATADELAY
typedef uint8_t sdio_eventset_t;

struct mmcsd_state_s;
struct mmcsd_part_s { struct mmcsd_state_s *priv; };
enum action { LOCK, SEND55, POLL55, R155, SENDAC23, POLLAC23, R1AC23, BLOCK, ARM_DATA, SETUP, SEND25, POLL25, R125, EVENT, SEND12, POLL12, R112, ARM_WR, UNLOCK };
struct fake { unsigned phase, stop_count, wait_count, cancel_count, wrarm_count, unlock_count; bool held, timeout, stop_error; const unsigned char *buffer; };
struct mmcsd_state_s {
  struct mmcsd_part_s part[1];
  struct fake *dev;
  unsigned blocksize, blockshift, partnum, selblocklen, rca, type, caps;
  bool cmd23support, locked, wrbusy, wrprotect;
};
struct inode { void *i_private; };

static void step(struct fake *fake, enum action a) {
  CHECK(fake->phase == (unsigned)a);
  CHECK(a == LOCK ? !fake->held : fake->held);
  fake->phase++;
}
static int mmcsd_lock(struct mmcsd_state_s *p) {
  step(p->dev, LOCK);
  p->dev->held = true;
  return OK;
}
static void mmcsd_unlock(struct mmcsd_state_s *p) {
  if (p->dev->phase == ARM_WR) {
    p->dev->phase++;
  }
  step(p->dev, UNLOCK);
  p->dev->held = false;
  p->dev->unlock_count++;
}
static int mmcsd_transferready(struct mmcsd_state_s *p) {
  CHECK(p->dev->held && !p->wrbusy);
  return OK;
}
static int mmcsd_switch(struct mmcsd_state_s *p, uint32_t arg) {
  (void)p; (void)arg;
  CHECK(false);
  return -EIO;
}
static int mmcsd_setblockcount(struct mmcsd_state_s *p, size_t count) {
  (void)p; (void)count;
  CHECK(false); /* Standalone CMD23 must not occur. */
  return -EIO;
}
static ssize_t mmcsd_writesingle(struct mmcsd_part_s *p, const uint8_t *b, off_t sector) {
  (void)p; (void)b; (void)sector;
  CHECK(false);
  return -EIO;
}
static int send_cmd(struct fake *f, uint32_t cmd, uint32_t arg) {
  switch (cmd) {
    case SD_CMD55: step(f, SEND55); CHECK(arg == (3u << 16)); break;
    case SD_ACMD23: step(f, SENDAC23); CHECK(arg == 2); break;
    case MMCSD_CMD25: step(f, SEND25); CHECK(arg == 11); break;
    case MMCSD_CMD12: step(f, SEND12); CHECK(arg == 0); f->stop_count++; break;
    default: CHECK(false);
  }
  return OK;
}
static int poll_resp(struct fake *f, uint32_t cmd) {
  switch (cmd) {
    case SD_CMD55: step(f, POLL55); break;
    case SD_ACMD23: step(f, POLLAC23); break;
    case MMCSD_CMD25: step(f, POLL25); break;
    case MMCSD_CMD12: step(f, POLL12); break;
    default: CHECK(false);
  }
  return OK;
}
static int recv_r1(struct fake *f, uint32_t cmd, uint32_t *r1) {
  *r1 = 0;
  switch (cmd) {
    case SD_CMD55: step(f, R155); break;
    case SD_ACMD23: step(f, R1AC23); break;
    case MMCSD_CMD25: step(f, R125); break;
    case MMCSD_CMD12:
      step(f, R112);
      if (f->stop_error) *r1 = MMCSD_R1_ERROR;
      break;
    default: CHECK(false);
  }
  return OK;
}
static void block_setup(struct fake *f, unsigned size, size_t count) {
  step(f, BLOCK);
  CHECK(size == 512 && count == 2);
}
static void arm(struct fake *f, sdio_eventset_t events, unsigned timeout) {
  CHECK(timeout == 2u * PINNED_WDATADELAY);
#ifdef CONFIG_MMCSD_SDIOWAIT_WRCOMPLETE
  if (events == (SDIOWAIT_WRCOMPLETE | SDIOWAIT_TIMEOUT)) {
    step(f, ARM_WR);
    f->wrarm_count++;
    return;
  }
#endif
  step(f, ARM_DATA);
  CHECK(events == (SDIOWAIT_TRANSFERDONE | SDIOWAIT_TIMEOUT | SDIOWAIT_ERROR));
}
static void prepare_send(struct fake *f, const uint8_t *buffer, size_t nbytes) {
  step(f, SETUP);
  CHECK(buffer == f->buffer && nbytes == 1024);
}
static sdio_eventset_t await_data(struct fake *f) {
  step(f, EVENT);
  f->wait_count++;
  return f->timeout ? SDIOWAIT_TIMEOUT : SDIOWAIT_TRANSFERDONE;
}
#define SDIO_SENDCMD send_cmd
#define SDIO_WAITRESPONSE poll_resp
#define SDIO_RECVR1 recv_r1
#define SDIO_WRPROTECTED(dev) ((void)(dev), false)
#define SDIO_BLOCKSETUP block_setup
#define SDIO_WAITENABLE arm
#define SDIO_SENDSETUP prepare_send
#define SDIO_EVENTWAIT await_data
#define SDIO_CANCEL(dev) ((dev)->cancel_count++)

@PINNED_SOURCE@

static void run_case(const char *name, bool timeout, bool stop_error, ssize_t expected) {
  unsigned char bytes[1024] = {0};
  struct fake f = {.timeout = timeout, .stop_error = stop_error, .buffer = bytes};
  struct mmcsd_state_s p = {
    .dev = &f, .blocksize = 512, .selblocklen = 512, .blockshift = 9,
    .rca = 3, .type = MMCSD_CARDTYPE_SDV2 | MMCSD_CARDTYPE_BLOCK,
    .caps = SDIO_CAPS_DMABEFOREWRITE,
  };
  struct inode inode = {.i_private = &p.part[0]};
  p.part[0].priv = &p;
  ssize_t actual = mmcsd_write(&inode, bytes, 11, 2);
  CHECK(actual == expected);
  CHECK(f.phase == UNLOCK + 1 && !f.held);
  CHECK(f.stop_count == 1 && f.wait_count == 1 && f.unlock_count == 1 && f.cancel_count == 0);
  CHECK(p.wrbusy == (expected == 2));
#ifdef CONFIG_MMCSD_SDIOWAIT_WRCOMPLETE
  CHECK(f.wrarm_count == (unsigned)(expected == 2));
#else
  CHECK(f.wrarm_count == 0);
#endif
  printf("%s: ret=%zd stop=%u datawait=%u cancel=%u wrbusy=%d wrarm=%u unlock=%u\n",
    name, actual, f.stop_count, f.wait_count, f.cancel_count, p.wrbusy, f.wrarm_count, f.unlock_count);
}
int main(void) {
  run_case("DATA timeout, STOP R1 error", true, true, -ETIMEDOUT);
  run_case("DATA done, STOP R1 error", false, true, -EIO);
  run_case("DATA done, STOP success", false, false, 2);
  return 0;
}
```

### test.log

```text
cc659e003afd1ec1fd4fee3d7b1a0b807bdc4270  drivers/mmcsd/mmcsd_sdio.c
23a2550ef0b0ec5dd542d0a24e478c6d0a24cd99  include/nuttx/sdio.h
76dcf15c6c4922c97f7e5f3b1180b304506c1274  drivers/mmcsd/mmcsd_sdio.h
ff20eb283a56f61226116486fe733b30153ff55e  drivers/mmcsd/mmcsd.h
e550fdcbe648eadd8e358996eeccb15814ce035d  drivers/mmcsd/Kconfig
gcc (Debian 14.2.0-19) 14.2.0
-O0 WRCOMPLETE=0
DATA timeout, STOP R1 error: ret=-110 stop=1 datawait=1 cancel=0 wrbusy=0 wrarm=0 unlock=1
DATA done, STOP R1 error: ret=-5 stop=1 datawait=1 cancel=0 wrbusy=0 wrarm=0 unlock=1
DATA done, STOP success: ret=2 stop=1 datawait=1 cancel=0 wrbusy=1 wrarm=0 unlock=1
-O0 WRCOMPLETE=1
DATA timeout, STOP R1 error: ret=-110 stop=1 datawait=1 cancel=0 wrbusy=0 wrarm=0 unlock=1
DATA done, STOP R1 error: ret=-5 stop=1 datawait=1 cancel=0 wrbusy=0 wrarm=0 unlock=1
DATA done, STOP success: ret=2 stop=1 datawait=1 cancel=0 wrbusy=1 wrarm=1 unlock=1
-O2 WRCOMPLETE=0
DATA timeout, STOP R1 error: ret=-110 stop=1 datawait=1 cancel=0 wrbusy=0 wrarm=0 unlock=1
DATA done, STOP R1 error: ret=-5 stop=1 datawait=1 cancel=0 wrbusy=0 wrarm=0 unlock=1
DATA done, STOP success: ret=2 stop=1 datawait=1 cancel=0 wrbusy=1 wrarm=0 unlock=1
-O2 WRCOMPLETE=1
DATA timeout, STOP R1 error: ret=-110 stop=1 datawait=1 cancel=0 wrbusy=0 wrarm=0 unlock=1
DATA done, STOP R1 error: ret=-5 stop=1 datawait=1 cancel=0 wrbusy=0 wrarm=0 unlock=1
DATA done, STOP success: ret=2 stop=1 datawait=1 cancel=0 wrbusy=1 wrarm=1 unlock=1
```
