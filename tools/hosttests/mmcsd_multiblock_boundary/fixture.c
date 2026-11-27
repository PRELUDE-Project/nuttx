/* SPDX-License-Identifier: Apache-2.0 */
/* 固定したCMD25上位関数を使い、媒体と下位転送は模擬する。 */
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <errno.h>
#include <stdio.h>
#include <sys/types.h>

#define FAR
#define OK 0
#define finfo(...) ((void)0)
#define ferr(...) ((void)0)
#define DEBUGASSERT assert
#define SDIO_CAPS_DMASUPPORTED 1
#define SDIO_CAPS_DMABEFOREWRITE 2
#define SDIOWAIT_TRANSFERDONE 1
#define SDIOWAIT_TIMEOUT 2
#define SDIOWAIT_ERROR 4
#define SDIOWAIT_WRCOMPLETE 8
#define MMCSD_BLOCK_WDATADELAY 100
#define MMC_CMD6_MODE_WRITE_BYTE 0
#define EXT_CSD_PART_CONF 0
#define MMC_CMD6_MODE(x) (x)
#define MMC_CMD6_INDEX(x) (x)
#define MMC_CMD6_VALUE(x) (x)
#define MMCSD_CMD25 25
#define SD_CMD55 55
#define SD_ACMD23 123
#define IS_SD(x) ((x) == 1)
#define IS_BLOCK(x) ((x) == 1)

struct mmcsd_state_s;
struct mmcsd_part_s { struct mmcsd_state_s *priv; };
struct mmcsd_state_s
{
  struct mmcsd_part_s part[1];
  uint32_t partnum;
  int blockshift, blocksize, caps, type;
  uint16_t rca;
  void *dev;
  bool wrbusy, cmd23support;
};

static int cmd25, set_count, stop_count, transfer_wait, completion_wait, setup;
static int event_error, stop_error;
static bool dma_expected;

static int mmcsd_wrprotected(struct mmcsd_state_s *s) { return 0; }
static int mmcsd_switch(struct mmcsd_state_s *s, int a) { assert(!"unexpected partition switch"); return -EIO; }
static int mmcsd_transferready(struct mmcsd_state_s *s) { assert(!s->wrbusy); return 0; }
static int mmcsd_setblocklen(struct mmcsd_state_s *s, int n) { assert(n == 512); return 0; }
static int mmcsd_sendcmdpoll(struct mmcsd_state_s *s, int c, off_t a)
{
  if (c == SD_CMD55) assert(a == ((uint32_t)s->rca << 16));
  else if (c == SD_ACMD23) assert(a == 2);
  else
    {
      assert(c == MMCSD_CMD25 && a == 7);
      /* 能力ビットによってデータ設定とコマンドの順が変わる。 */
      assert(setup == !!(s->caps & SDIO_CAPS_DMABEFOREWRITE));
      cmd25++;
    }
  return 0;
}
static int mmcsd_recv_r1(struct mmcsd_state_s *s, int c) { assert(c == SD_CMD55 || c == SD_ACMD23 || c == MMCSD_CMD25); return 0; }
static int mmcsd_setblockcount(struct mmcsd_state_s *s, size_t n) { assert(n == 2); set_count++; return 0; }
static int mmcsd_eventwait(struct mmcsd_state_s *s, int e)
{
  assert(e == (SDIOWAIT_TIMEOUT | SDIOWAIT_ERROR) && setup == 1 && cmd25 == 1);
  transfer_wait++;
  return event_error;
}
static int mmcsd_stoptransmission(struct mmcsd_state_s *s) { assert(transfer_wait == 1); stop_count++; return stop_error; }
static void wait_enable(int e, int t)
{
  assert(t == 2 * MMCSD_BLOCK_WDATADELAY);
  if (e & SDIOWAIT_WRCOMPLETE) { assert(transfer_wait == 1); completion_wait++; }
  else assert(e == (SDIOWAIT_TRANSFERDONE | SDIOWAIT_TIMEOUT | SDIOWAIT_ERROR));
}
static int send_setup(const uint8_t *p, size_t n, bool dma)
{
  assert(p && n == 1024 && dma == dma_expected);
  setup++;
  return 0;
}
#define SDIO_BLOCKSETUP(d,b,n) assert((b) == 512 && (n) == 2)
#define SDIO_WAITENABLE(d,e,t) wait_enable(e,t)
#define SDIO_DMASENDSETUP(d,b,n) send_setup(b,n,true)
#define SDIO_SENDSETUP(d,b,n) send_setup(b,n,false)
#define SDIO_CANCEL(d) assert(!"unexpected cancel in this boundary")
#include "actual.c.inc"

int main(void)
{
  const uint8_t data[1024] = {0};
  int cases = 0;
  const int statuses[] = {0, -ETIMEDOUT, -EIO};
  for (int caps = 0; caps < 4; caps++)
    {
#ifndef CONFIG_SDIO_DMA
      if (caps & SDIO_CAPS_DMASUPPORTED) continue;
#endif
      for (int cmd23 = 0; cmd23 < 2; cmd23++)
        for (size_t i = 0; i < sizeof(statuses)/sizeof(statuses[0]); i++)
          for (int stop_fails = 0; stop_fails < 2; stop_fails++)
            {
              struct mmcsd_state_s card = {.type=1,.blocksize=512,.blockshift=9,.rca=7,.caps=caps,.cmd23support=cmd23};
              card.part[0].priv = &card;
              cmd25 = set_count = stop_count = transfer_wait = completion_wait = setup = 0;
              event_error = statuses[i];
              stop_error = stop_fails ? -EILSEQ : 0;
              dma_expected = (caps & SDIO_CAPS_DMASUPPORTED) != 0;
              /* CMD12を試行しても、先に起きたデータ転送エラーを返す。 */
              const int want = event_error ? event_error :
                               (!cmd23 && stop_fails ? -EILSEQ : 2);
              assert(mmcsd_writemultiple(card.part, data, 7, 2) == want);
              assert(cmd25 == 1 && transfer_wait == 1 && setup == 1);
              assert(set_count == cmd23 && stop_count == !cmd23);
              /* エラー時は次の処理向けに完了待ちを登録しない。 */
              assert(card.wrbusy == (want == 2));
#ifdef CONFIG_MMCSD_SDIOWAIT_WRCOMPLETE
              assert(completion_wait == (want == 2));
#else
              assert(completion_wait == 0);
#endif
              cases++;
            }
    }
  printf("CMD25 boundary PASS cases=%d hardware=none\n", cases);
  return 0;
}
