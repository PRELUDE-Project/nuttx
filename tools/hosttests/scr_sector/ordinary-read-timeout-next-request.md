# 単一セクタ読み取りの転送エラーと次の要求

同じMMCSDインスタンスに、セクタ18と19の独立した読み取り要求を1件ずつ出しました。どちらもCMD17のコマンド応答までは成功するようにし、1件目のデータ完了イベントだけを変えました。DMAと診断用の受信隔離設定は使っていません。

| 1件目の模擬イベント | 1件目の戻り値 | 次の独立した要求 |
| --- | --- | --- |
| タイムアウト | `-ETIMEDOUT` | 1ブロック成功 |
| データエラー | `-EIO` | 1ブロック成功 |
| 転送完了とデータエラー | `-EIO` | 1ブロック成功 |
| タイムアウトとデータエラー | `-ETIMEDOUT` | 1ブロック成功 |

各条件でロック・解放、CMD17、受信設定、待機は各2回でした。上位によるエラー要求の自動再試行や追加の `SDIO_CANCEL` はありません。データエラー付きの条件では、下位が先頭7バイトだけ書き込んでからエラーを通知するよう模擬しました。上位は1件目のバッファを元に戻さないため、エラー時の中身を完全な読出しデータとして扱うことはできません。2件目は別のバッファに全512バイトが揃うことを確認しました。

GCC 14.2.0、UBSan有効の `-O0` と `-O2` で4条件ずつ通過しました。これはNuttX 12.10の上位処理の試験で、CRCエラーを検出する実IRQやDMA、FATの書込み経路、実カードが次の要求に応答する条件は確認していません。

## 再現手順

[固定した `mmcsd_sdio.c`](https://github.com/PRELUDE-Project/nuttx/blob/d563d3faf6629352cab248d032b422ee1e86b996/drivers/mmcsd/mmcsd_sdio.c) と下記 `fixture.in` を作業用ディレクトリに置き、次を実行します。

```sh
test "$(git hash-object mmcsd_sdio.c)" = cc659e003afd1ec1fd4fee3d7b1a0b807bdc4270
python3 - <<'PY'
from pathlib import Path
s = Path('mmcsd_sdio.c').read_text().splitlines()
parts = ['\n'.join(s[a-1:b]) for a,b in [(1370,1409),(1634,1778),(2516,2585)]]
f = Path('fixture.in').read_text()
assert f.count('__PINNED_FUNCTIONS__') == 1
Path('fixture.c').write_text(f.replace('__PINNED_FUNCTIONS__','\n\n'.join(parts)))
PY
printf '%s  %s\n' 790e37b1be320a66a7dcb4acbdae296055bbf18fa3c7d71f451d9985ba73db0c fixture.c | sha256sum -c -
for opt in 0 2; do
  gcc -O"$opt" -std=c11 -Wall -Wextra -Werror -fsanitize=undefined \
    -fno-sanitize-recover=undefined fixture.c -o "run$opt"
  "./run$opt"
done
```

`fixture.in`

```c
#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#define FAR
#define OK 0
#define DEBUGASSERT(x) assert(x)
#define finfo(...) ((void)0)
#define ferr(...) ((void)0)
#define MMCSD_MULTIBLOCK_LIMIT 1
#define MMCSD_CMD17 17
#define MMCSD_BLOCK_RDATADELAY 100
#define MMC_CMD6_MODE(x) (x)
#define MMC_CMD6_MODE_WRITE_BYTE 3u
#define MMC_CMD6_INDEX(x) ((x)<<8)
#define EXT_CSD_PART_CONF 179u
#define MMC_CMD6_VALUE(x) ((x)<<16)
#define IS_BLOCK(x) ((x)==1)
#define SDIOWAIT_TRANSFERDONE 4
#define SDIOWAIT_TIMEOUT 8
#define SDIOWAIT_ERROR 16
typedef unsigned sdio_eventset_t;
#define blkcnt_t uint32_t
struct mmcsd_state_s;
struct mmcsd_part_s {struct mmcsd_state_s *priv;};
struct mmcsd_state_s {
  struct mmcsd_part_s part[1]; unsigned partnum, blocksize, blockshift, type;
  int locked; void *dev;
};
struct inode { void *i_private; };
static unsigned locks, unlocks, depth, stage, setup, waits, cmd17, cancels;
static off_t addresses[2];
static unsigned char *pending;
static sdio_eventset_t first_event;
static int mmcsd_lock(struct mmcsd_state_s *p){assert(p->dev==p&&depth==0&&stage==0);++locks;++depth;return 0;}
static void mmcsd_unlock(struct mmcsd_state_s *p){assert(p->dev==p&&depth==1&&stage==0);--depth;++unlocks;}
static int mmcsd_switch(struct mmcsd_state_s *p,unsigned v){(void)p;(void)v;assert(0);return -EIO;}
static int mmcsd_transferready(struct mmcsd_state_s *p){assert(p->dev==p&&depth==1&&stage==0);return 0;}
static int mmcsd_setblocklen(struct mmcsd_state_s *p,unsigned n){assert(p->dev==p&&n==512&&stage==0);return 0;}
static void SDIO_BLOCKSETUP(void *p,unsigned n,unsigned d){assert(p&&n==512&&d==1&&stage++==0);}
static void SDIO_WAITENABLE(void *p,unsigned ev,unsigned t){assert(p&&ev==28&&t==100&&stage++==1);}
static int SDIO_RECVSETUP(void *p,unsigned char *buffer,unsigned n){assert(p&&buffer&&n==512&&stage++==2);++setup;pending=buffer;return 0;}
static void SDIO_CANCEL(void *p){assert(p);++cancels;}
static void mmcsd_sendcmdpoll(struct mmcsd_state_s *p,unsigned cmd,off_t addr){assert(p&&cmd==17&&cmd17<2&&stage++==3);addresses[cmd17++]=addr;}
static int mmcsd_recv_r1(struct mmcsd_state_s *p,unsigned cmd){assert(p&&cmd==17&&stage++==4);return 0;}
static sdio_eventset_t SDIO_EVENTWAIT(void *p){
  assert(p&&depth==1&&stage==5&&pending&&waits<2);
  stage=0;
  if (++waits==1) {
    /* エラーになる前に一部を書き込んだ下位受信を模擬する。 */
    if (first_event & SDIOWAIT_ERROR) memset(pending,0x6b,7);
    pending=NULL;
    return first_event;
  }
  memset(pending,0xa5,512); pending=NULL;
  return SDIOWAIT_TRANSFERDONE;
}

__PINNED_FUNCTIONS__

static void run_case(sdio_eventset_t event, ssize_t expected){
  locks=unlocks=depth=stage=setup=waits=cmd17=cancels=0;
  first_event=event;
  struct mmcsd_state_s p={.partnum=0,.blocksize=512,.blockshift=9,.type=1};
  p.dev=&p;p.part[0].priv=&p;
  struct inode i={.i_private=&p.part[0]};
  unsigned char a[512]={0},b[512]={0};
  assert(mmcsd_read(&i,a,18,1)==expected);
  for (size_t j=0;j<512;++j)
    assert(a[j]==((event & SDIOWAIT_ERROR) && j<7 ? 0x6b : 0));
  assert(depth==0&&locks==1&&unlocks==1&&addresses[0]==18&&cmd17==1&&waits==1&&setup==1&&cancels==0);
  assert(mmcsd_read(&i,b,19,1)==1);
  assert(depth==0&&locks==2&&unlocks==2&&addresses[1]==19&&cmd17==2&&waits==2&&setup==2&&cancels==0);
  for (size_t j=0;j<512;++j) assert(b[j]==0xa5);
  printf("PASS CMD17 first_event=%u first_result=%zd next_result=1 retry=0 cancel=0\n",
         event, expected);
}

int main(void){
  run_case(SDIOWAIT_TIMEOUT,-ETIMEDOUT);
  run_case(SDIOWAIT_ERROR,-EIO);
  run_case(SDIOWAIT_TRANSFERDONE|SDIOWAIT_ERROR,-EIO);
  run_case(SDIOWAIT_TIMEOUT|SDIOWAIT_ERROR,-ETIMEDOUT);
  return 0;
}
```
