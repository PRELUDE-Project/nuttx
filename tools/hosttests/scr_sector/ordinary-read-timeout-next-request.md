# 単一セクタ読み取りのタイムアウトと次の要求

同じMMCSDインスタンスに、セクタ18と19の独立した読み取り要求を1件ずつ出しました。SDIOは模擬し、両方でカード準備とCMD17の応答を成功させたうえで、データ完了イベントだけをタイムアウト、正常完了の順に返しました。DMAと診断用の受信隔離設定は使っていません。

1件目は `-ETIMEDOUT` を返してロックを解放し、2件目はセクタ19の1ブロックを返しました。ロックと解放は各2回、CMD17・受信設定・待機も各2回で、タイムアウトした要求の自動再試行や追加の `SDIO_CANCEL` 呼び出しはありませんでした。GCC 14.2.0、UBSan有効の `-O0` と `-O2` の両方で一致しました。

検証対象は上位の読み取り処理です。2件目の正常応答とバッファへの書込みは模擬SDIOが提供したため、実際のカードが次の要求に応答することや、H753のIRQ・DMA・バッファ寿命はこの試験では確認していません。

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
printf '%s  %s\n' fa5e29c2d5b8a0f790cc2fca280959f0c3c7e82240767120e1feaffc3c5e1339 fixture.c | sha256sum -c -
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
  if (++waits==1) {pending=NULL;return SDIOWAIT_TIMEOUT;}
  memset(pending,0xa5,512); pending=NULL;
  return SDIOWAIT_TRANSFERDONE;
}

__PINNED_FUNCTIONS__

int main(void){
  struct mmcsd_state_s p={.partnum=0,.blocksize=512,.blockshift=9,.type=1};
  p.dev=&p;p.part[0].priv=&p;
  struct inode i={.i_private=&p.part[0]};
  unsigned char a[512]={0},b[512]={0};
  assert(mmcsd_read(&i,a,18,1)==-ETIMEDOUT);
  assert(depth==0&&locks==1&&unlocks==1&&addresses[0]==18&&cmd17==1&&waits==1&&setup==1&&cancels==0);
  assert(mmcsd_read(&i,b,19,1)==1);
  assert(depth==0&&locks==2&&unlocks==2&&addresses[1]==19&&cmd17==2&&waits==2&&setup==2&&cancels==0);
  for (size_t j=0;j<512;++j) assert(b[j]==0xa5);
  puts("PASS: same instance, ordinary CMD17; timeout -> -ETIMEDOUT, next request -> 1; two locks/unlocks, two CMD17, two waits, no implicit retry/CANCEL");
}
```
