# 通常の1ブロック書き込み：CMD24がエラー応答を返した場合

STM32H7ではIDMAを使わない構成でも、下位ドライバーは[送信準備を先に行う特性を返します](https://github.com/PRELUDE-Project/nuttx/blob/7965c77ad63db5ed14506ca19f112ac6df04fa7e/arch/arm/src/stm32h7/stm32_sdmmc.c#L2671-L2687)。この経路を模擬し、送信準備後のCMD24に一般エラーを含むR1応答が返った場合を、[固定版の上位処理](https://github.com/PRELUDE-Project/nuttx/blob/7965c77ad63db5ed14506ca19f112ac6df04fa7e/drivers/mmcsd/mmcsd_sdio.c#L2064-L2147)で確認しました。

| R1応答 | 上位の返り値 | キャンセル | 転送完了待ち | 書き込み中フラグ |
| --- | --- | ---: | ---: | ---: |
| 一般エラー | `-EIO` | 1回 | 0回 | 0 |
| 正常 | 1ブロック | 0回 | 1回 | 1 |

両条件ともロックと解放は各1回です。異常時のキャンセルはロック中に行われ、完了待ちには進みません。GCC 14.2.0の `-O0`、`-O2`、UBSanで合格しました。前工程の[通常読み取りタイムアウト](ordinary-read-timeout-next-request.md)とは別の試験です。

対象は上位の通常PIO書き込み、R1のデコード、完了待ちへ進むかどうかです。SDIO、初期化などの補助処理、カード応答は模擬で、DMA・書き込み完了専用待ち・実機は検証していません。キャンセルの呼び出しを確認しただけで、下位のレジスタや割り込みが止まったこと、カードにデータが残ったこと、次回の書き込み成功は保証しません。

## 再現

作業用の空ディレクトリに `7965c77a` の `drivers/mmcsd/mmcsd_sdio.c` と以下の `run.sh`、`fixture.in` を置き、`sh run.sh` を実行します。固定ファイルのGit blobが異なる場合は停止します。

### run.sh

```sh
#!/bin/sh
set -eu
cd "$(dirname "$0")"
test "$(git hash-object mmcsd_sdio.c)" = cc659e003afd1ec1fd4fee3d7b1a0b807bdc4270
python3 - <<'PY'
from pathlib import Path
src=Path('mmcsd_sdio.c').read_text().splitlines()
slices=[(413,438),(1370,1409),(1971,2148),(2596,2666)]
stub=Path('fixture.in').read_text()
if stub.count('__PINNED_FUNCTIONS__') != 1:
    raise SystemExit('invalid fixture marker')
Path('fixture.c').write_text(stub.replace('__PINNED_FUNCTIONS__','\n\n'.join('\n'.join(src[a-1:b]) for a,b in slices)))
PY
gcc --version | head -n1
git hash-object mmcsd_sdio.c
sha256sum mmcsd_sdio.c fixture.in fixture.c run.sh
for opt in 0 2; do
  printf 'gcc -O%s -std=c11 -Wall -Wextra -Werror -fsanitize=undefined -fno-sanitize-recover=undefined fixture.c -o run%s\n' "$opt" "$opt"
  gcc -O"$opt" -std=c11 -Wall -Wextra -Werror -fsanitize=undefined -fno-sanitize-recover=undefined fixture.c -o "run$opt"
  "./run$opt"
done
```

### fixture.in

```c
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "CHECK failed line %d: %s\n", __LINE__, #x); abort(); } } while (0)
#define FAR
#define OK 0
#define DEBUGASSERT(x) CHECK(x)
#define finfo(...) ((void)0)
#define ferr(...) ((void)0)
#define MMCSD_MULTIBLOCK_LIMIT 1
#define MMCSD_CMDIDX24 24
#define MMCSD_R1_RESPONSE (1u << 6)
#define MMCSD_WRDATAXFR ((1u << 10) | (4u << 10))
#define MMCSD_CMD24 (MMCSD_CMDIDX24 | MMCSD_R1_RESPONSE | MMCSD_WRDATAXFR)
#define MMCSD_CARDTYPE_BLOCK 8
#define IS_BLOCK(t) (((t) & MMCSD_CARDTYPE_BLOCK) != 0)
#define SDIO_CAPS_DMABEFOREWRITE 0x04
#define MMCSD_BLOCK_WDATADELAY 260
#define SDIOWAIT_TRANSFERDONE (1 << 2)
#define SDIOWAIT_TIMEOUT (1 << 3)
#define SDIOWAIT_ERROR (1 << 4)
#define MMCSD_R1_CARDISLOCKED (UINT32_C(1) << 25)
#define MMCSD_R1_ERROR (UINT32_C(1) << 19)
#define MMCSD_R1_ERRORMASK UINT32_C(0xfdffe088)
#define MMC_CMD6_MODE(a) ((uint32_t)(a) << 24)
#define MMC_CMD6_INDEX(a) ((uint32_t)(a) << 16)
#define MMC_CMD6_VALUE(a) ((uint32_t)(a) << 8)
#define MMC_CMD6_MODE_WRITE_BYTE 3
#define EXT_CSD_PART_CONF 179
typedef uint32_t nuttx_blkcnt_t;
#define blkcnt_t nuttx_blkcnt_t
typedef uint8_t sdio_eventset_t;
struct mmcsd_state_s;
struct mmcsd_part_s { struct mmcsd_state_s *priv; };
struct mmcsd_state_s {
  struct mmcsd_part_s part[1];
  unsigned partnum, blocksize, blockshift, type, caps;
  bool locked, wrbusy;
  struct mmcsd_state_s *dev;
};
struct inode { void *i_private; };
enum phase { FRESH, LOCKED, CHECKED, READY, BLOCKLEN, BLOCK, ENABLED,
             SEND, SENT_CMD, R1, CANCELLED, DONE, UNLOCKED };
static struct {
  enum phase phase;
  unsigned lock, unlock, block, enable, setup, command, r1, cancel, wait;
  bool should_fail;
  const unsigned char *expected;
  off_t sector;
} m;
static void step(struct mmcsd_state_s *p, enum phase prev, enum phase cur)
{ CHECK(p && p->dev == p && m.phase == prev); m.phase = cur; }
static int mmcsd_lock(struct mmcsd_state_s *p)
{ step(p,FRESH,LOCKED); ++m.lock; return OK; }
static void mmcsd_unlock(struct mmcsd_state_s *p)
{
  step(p,m.should_fail ? CANCELLED : DONE,UNLOCKED);
  CHECK(p->wrbusy == !m.should_fail);
  ++m.unlock;
}
static bool mmcsd_wrprotected(struct mmcsd_state_s *p)
{ step(p,LOCKED,CHECKED); return false; }
static int mmcsd_transferready(struct mmcsd_state_s *p)
{ step(p,CHECKED,READY); CHECK(!p->wrbusy); return OK; }
static int mmcsd_switch(struct mmcsd_state_s *p, uint32_t arg)
{ (void)p; (void)arg; CHECK(false); return -EIO; }
static int mmcsd_setblocklen(struct mmcsd_state_s *p, unsigned n)
{ step(p,READY,BLOCKLEN); CHECK(n==512); return OK; }
static void SDIO_BLOCKSETUP(struct mmcsd_state_s *p, unsigned size, unsigned blocks)
{ step(p,BLOCKLEN,BLOCK); CHECK(size==512 && blocks==1); ++m.block; }
static void SDIO_WAITENABLE(struct mmcsd_state_s *p, unsigned mask, unsigned delay)
{
  step(p,BLOCK,ENABLED);
  CHECK(mask==(SDIOWAIT_TRANSFERDONE|SDIOWAIT_TIMEOUT|SDIOWAIT_ERROR) && delay==260);
  ++m.enable;
}
static void SDIO_SENDSETUP(struct mmcsd_state_s *p, const unsigned char *buf, unsigned n)
{ step(p,ENABLED,SEND); CHECK(n==512 && buf==m.expected); ++m.setup; }
static int mmcsd_sendcmdpoll(struct mmcsd_state_s *p, unsigned cmd, off_t offset)
{ step(p,SEND,SENT_CMD); CHECK(cmd==MMCSD_CMD24 && offset==m.sector); ++m.command; return OK; }
static int SDIO_RECVR1(struct mmcsd_state_s *p, unsigned cmd, uint32_t *r1)
{ step(p,SENT_CMD,R1); CHECK(cmd==MMCSD_CMD24 && r1); ++m.r1; *r1=m.should_fail ? MMCSD_R1_ERROR : 0; return OK; }
static void SDIO_CANCEL(struct mmcsd_state_s *p)
{ step(p,R1,CANCELLED); CHECK(m.should_fail && m.lock==1 && m.unlock==0 && !p->wrbusy); ++m.cancel; }
static sdio_eventset_t SDIO_EVENTWAIT(struct mmcsd_state_s *p)
{ step(p,R1,DONE); CHECK(!m.should_fail && m.lock==1 && m.unlock==0); ++m.wait; return SDIOWAIT_TRANSFERDONE; }

__PINNED_FUNCTIONS__

static void run(bool fail)
{
  struct mmcsd_state_s p={.blocksize=512,.blockshift=9,.type=MMCSD_CARDTYPE_BLOCK,
                          .caps=SDIO_CAPS_DMABEFOREWRITE};
  p.dev=&p; p.part[0].priv=&p;
  struct inode i={.i_private=&p.part[0]};
  unsigned char buffer[512]={0};
  m.should_fail=fail; m.expected=buffer; m.sector=fail ? 18 : 19;
  ssize_t result=mmcsd_write(&i,buffer,m.sector,1);
  CHECK(result==(fail ? -EIO : 1));
  CHECK(m.phase==UNLOCKED && m.lock==1 && m.unlock==1);
  CHECK(m.block==1 && m.enable==1 && m.setup==1 && m.command==1 && m.r1==1);
  CHECK(m.cancel==(unsigned)fail && m.wait==(unsigned)!fail && p.wrbusy==!fail);
  printf("%s: result=%zd locks=%u unlocks=%u block=%u enable=%u setup=%u CMD24=%u R1=%u CANCEL=%u EVENTWAIT=%u wrbusy=%u\n",
         fail ? "R1 error" : "normal", result,m.lock,m.unlock,m.block,m.enable,m.setup,m.command,m.r1,m.cancel,m.wait,p.wrbusy);
  m.phase=FRESH; m.lock=m.unlock=m.block=m.enable=m.setup=m.command=m.r1=m.cancel=m.wait=0;
}
int main(void) { run(true); run(false); puts("PASS: real upper PIO write + R1 decoder with mocked SDIO; no media/device"); return 0; }
```
