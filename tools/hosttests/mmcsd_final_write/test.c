/* SPDX-License-Identifier: Apache-2.0 */
/* Actual MMC/SD upper functions, controlled time and transport only. */

#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/types.h>
#include <time.h>

#include "status-bits.h"

#define FAR
#define OK 0
#define finfo(...) ((void)0)
#define ferr(...) ((void)0)
#define fwarn(...) ((void)0)
#define DEBUGASSERT assert
#define SDIO_CAPS_DMASUPPORTED 1
#define SDIO_CAPS_DMABEFOREWRITE 2
#define MMC_CMD6_MODE_WRITE_BYTE 0
#define EXT_CSD_PART_CONF 0
#define MMC_CMD6_MODE(x) (x)
#define MMC_CMD6_INDEX(x) (x)
#define MMC_CMD6_VALUE(x) (x)
#define MMCSD_CMD13 13
#define MMCSD_CMD24 24
#define SDIOWAIT_TRANSFERDONE 1
#define SDIOWAIT_TIMEOUT 2
#define SDIOWAIT_ERROR 4
#define SDIOWAIT_WRCOMPLETE 8
#define MMCSD_BLOCK_WDATADELAY 100
#define TICK_PER_SEC 4
#define IS_BLOCK(t) ((t) == 1)
#define IS_EMPTY(p) ((p)->type == 0)
#define IS_STATE(r, s) (((r) & MMCSD_R1_STATE_MASK) == (s))

struct mmcsd_state_s;
struct mmcsd_part_s { struct mmcsd_state_s *priv; };
struct mmcsd_state_s
{
  struct mmcsd_part_s part[1];
  unsigned int partnum;
  int caps, type, blocksize, blockshift, crefs;
  uint16_t rca;
  void *dev;
  bool wrbusy, locked;
#ifdef CANDIDATE
  bool write_uncertain; /* fixture only */
#endif
};

struct inode { void *i_private; };

static bool present;
static bool locked;
static bool transfer_armed;
static bool completion_armed;
static int writes, status_sent, status_received, waits, wait_notice;
static int cmd13_error, r1_error, transfer_result, notice_result, lock_result;
static int trace_count, status_index;
static uint32_t status_trace[5];
static clock_t ticks;

static int mmcsd_lock(struct mmcsd_state_s *priv)
{
  assert(!locked);
  if (!lock_result)
    locked = true;
  return lock_result;
}

static void mmcsd_unlock(struct mmcsd_state_s *priv)
{
  assert(locked);
  locked = false;
}

static int mmcsd_wrprotected(struct mmcsd_state_s *priv)
{
  return priv->locked;
}

static int mmcsd_switch(struct mmcsd_state_s *priv, int argument)
{
  assert(!"unexpected partition switch");
  return -EIO;
}

static int mmcsd_setblocklen(struct mmcsd_state_s *priv, int size)
{
  assert(size == 512);
  return OK;
}

static int mmcsd_sendcmdpoll(struct mmcsd_state_s *priv, int cmd,
                             off_t argument)
{
  if (cmd == MMCSD_CMD24)
    {
      assert(argument == 7);
      writes++;
      return OK;
    }
  assert(cmd == MMCSD_CMD13 && argument == ((uint32_t)priv->rca << 16));
  status_sent++;
  return cmd13_error;
}

static int read_r1(void *dev, int cmd, uint32_t *r1)
{
  assert(cmd == MMCSD_CMD13);
  status_received++;
  if (r1_error)
    return r1_error;
  assert(trace_count > 0);
  *r1 = status_trace[status_index];
  if (status_index < trace_count - 1)
    status_index++;
  return OK;
}

static int mmcsd_recv_r1(struct mmcsd_state_s *priv, int cmd)
{
  assert(cmd == MMCSD_CMD24);
  return OK;
}

static void wait_enable(int events, int timeout)
{
  assert(timeout == MMCSD_BLOCK_WDATADELAY);
  if (events & SDIOWAIT_TRANSFERDONE)
    transfer_armed = true;
  else
    {
      assert(events & SDIOWAIT_WRCOMPLETE);
      completion_armed = true;
    }
}

static int mmcsd_eventwait(struct mmcsd_state_s *priv, int events)
{
  assert(events == (SDIOWAIT_TIMEOUT | SDIOWAIT_ERROR));
  waits++;
  if (transfer_armed)
    {
      transfer_armed = false;
      return transfer_result;
    }
  assert(completion_armed);
  completion_armed = false;
  wait_notice++;
  return notice_result;
}

static int send_setup(const uint8_t *buffer, size_t len)
{
  assert(buffer && len == 512 && transfer_armed);
  return OK;
}

static clock_t clock_systime_ticks(void) { return ticks; }
static void delay_tick(void) { ticks++; }

#define SDIO_PRESENT(d) present
#define SDIO_RECVR1(d, c, r) read_r1(d, c, r)
#define SDIO_BLOCKSETUP(d, b, n) assert((b) == 512 && (n) == 1)
#define SDIO_WAITENABLE(d, e, t) wait_enable(e, t)
#define SDIO_SENDSETUP(d, b, n) send_setup(b, n)
#define SDIO_DMASENDSETUP(d, b, n) send_setup(b, n)
#define SDIO_CANCEL(d) assert(!"unexpected cancel")
#define MMCSD_USLEEP(n) delay_tick()
#define sched_yield() delay_tick()

#include "actual.c.inc"

static void reset_fixture(void)
{
  present = true;
  locked = transfer_armed = completion_armed = false;
  writes = status_sent = status_received = waits = wait_notice = 0;
  cmd13_error = r1_error = transfer_result = notice_result = lock_result = 0;
  trace_count = 1;
  status_index = 0;
  status_trace[0] = MMCSD_R1_STATE_PRG;
  ticks = 0;
}

static void init_card(struct mmcsd_state_s *priv)
{
  *priv = (struct mmcsd_state_s){ .type = 1, .blocksize = 512,
          .blockshift = 9, .crefs = 1, .rca = 7 };
#ifdef CONFIG_SDIO_DMA
  priv->caps = SDIO_CAPS_DMASUPPORTED | SDIO_CAPS_DMABEFOREWRITE;
#endif
  priv->part[0].priv = priv;
}

static const uint8_t payload[512];

static void final_write_close(void)
{
  struct mmcsd_state_s card;
  reset_fixture();
  init_card(&card);
  struct inode inode = { card.part };
  assert(mmcsd_writesingle(card.part, payload, 7) == 1);
  assert(card.wrbusy && writes == 1 && waits == 1 && !status_sent);
  assert(mmcsd_close(&inode) == OK);
  assert(card.crefs == 0 && card.wrbusy && !locked);
  assert(writes == 1 && waits == 1 && !wait_notice && !status_sent);
}

static void status_after_write(int expected, uint32_t value, int poll_error,
                               int recv_error, bool is_present)
{
  struct mmcsd_state_s card;
  reset_fixture();
  init_card(&card);
  assert(mmcsd_writesingle(card.part, payload, 7) == 1);
  status_trace[0] = value;
  cmd13_error = poll_error;
  r1_error = recv_error;
  present = is_present;
  assert(mmcsd_transferready(&card) == expected);
  assert(card.wrbusy == (expected != OK));
  assert(writes == 1); /* No payload replay by the readiness check. */
  if (value & MMCSD_R1_WPVIOLATION)
    assert(card.locked == ((value & MMCSD_R1_CARDISLOCKED) != 0));
  if (!is_present)
    assert(!status_sent && !status_received && !wait_notice);
  else if (poll_error)
    assert(status_sent == 1 && !status_received);
  else if (expected == -ETIMEDOUT)
    assert(status_sent == TICK_PER_SEC && status_received == TICK_PER_SEC);
  else
    assert(status_sent == 1 && status_received == 1);
}

static void delayed_ready(void)
{
  struct mmcsd_state_s card;
  reset_fixture();
  init_card(&card);
  assert(mmcsd_writesingle(card.part, payload, 7) == 1);
  trace_count = 3;
  status_trace[1] = MMCSD_R1_STATE_RCV;
  status_trace[2] = MMCSD_R1_STATE_TRAN | MMCSD_R1_READYFORDATA;
#ifdef CANDIDATE
  trace_count = 4;
  status_trace[2] = MMCSD_R1_STATE_TRAN; /* Not READY_FOR_DATA yet. */
  status_trace[3] = MMCSD_R1_STATE_TRAN | MMCSD_R1_READYFORDATA;
#endif
  /* The existing optional write-complete IRQ wait is advisory: even a
   * timeout falls through to an independently successful CMD13. */
  notice_result = -ETIMEDOUT;
  assert(mmcsd_transferready(&card) == OK);
  assert(!card.wrbusy && writes == 1 && status_sent == trace_count &&
         status_received == trace_count && ticks == trace_count - 1);
#ifdef CONFIG_MMCSD_SDIOWAIT_WRCOMPLETE
  assert(wait_notice == 1);
#else
  assert(!wait_notice);
#endif
}

static void data_failure_and_close_lock_error(int injected_error)
{
  struct mmcsd_state_s card;
  reset_fixture();
  init_card(&card);
  struct inode inode = { card.part };
  transfer_result = injected_error;
  assert(mmcsd_writesingle(card.part, payload, 7) == injected_error);
  assert(!card.wrbusy && writes == 1 && waits == 1 && !status_sent);
  /* Nonbusy here is initialization state, not a hardware-stop result.
   * Even the actual readiness helper returns via its no-pending fast path. */
#ifdef CANDIDATE
  assert(card.write_uncertain);
  status_trace[0] = MMCSD_R1_STATE_TRAN | MMCSD_R1_READYFORDATA;
  assert(mmcsd_transferready(&card) == -EIO && !status_sent);

  /* Even the partition switch branch may not run after uncertainty. */
  card.partnum = 1;
  assert(mmcsd_writesingle(card.part, payload, 7) == -EIO);
  assert(card.write_uncertain && writes == 1 && waits == 1 && !status_sent);
  assert(mmcsd_close(&inode) == OK && card.crefs == 0 &&
         card.write_uncertain && !status_sent);
  card.crefs = 1; /* Reset reference counter only for independent lock test. */
#else
  assert(mmcsd_transferready(&card) == OK && !status_sent);
#endif
  lock_result = -EINTR;
  assert(mmcsd_close(&inode) == -EINTR);
  assert(card.crefs == 1 && !status_sent && !locked);
}

int main(void)
{
  final_write_close();
  delayed_ready();
  status_after_write(-ETIMEDOUT, MMCSD_R1_STATE_PRG, 0, 0, true);
  status_after_write(-ETIMEDOUT, MMCSD_R1_STATE_RCV, 0, 0, true);
  status_after_write(-EINVAL, MMCSD_R1_STATE_STBY, 0, 0, true);
  status_after_write(-EIO, MMCSD_R1_STATE_TRAN | MMCSD_R1_CARDISLOCKED | MMCSD_R1_WPVIOLATION,
                     0, 0, true);
  status_after_write(-EIO, MMCSD_R1_STATE_TRAN, -EIO, 0, true);
  status_after_write(-EILSEQ, MMCSD_R1_STATE_TRAN, 0, -EILSEQ, true);
  status_after_write(-ENODEV, MMCSD_R1_STATE_TRAN, 0, 0, false);
  /* Characterize current code, not an acceptance rule for a new provider:
   * TRAN without READY_FOR_DATA is still treated as successful. */
#ifdef CANDIDATE
  status_after_write(-ETIMEDOUT, MMCSD_R1_STATE_TRAN, 0, 0, true);
#else
  status_after_write(OK, MMCSD_R1_STATE_TRAN, 0, 0, true);
#endif
  status_after_write(OK, MMCSD_R1_STATE_TRAN | MMCSD_R1_READYFORDATA,
                     0, 0, true);
  data_failure_and_close_lock_error(-ETIMEDOUT);
  data_failure_and_close_lock_error(-EILSEQ);
  return 0;
}
