/* Host-only fake IRQ/register environment for extracted STM32H7 functions. */
/* SPDX-License-Identifier: Apache-2.0 */
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef unsigned int irqstate_t;
typedef uintptr_t wdparm_t;
typedef void (*wdentry_t)(wdparm_t);
typedef unsigned int sdio_eventset_t;
struct wdog_s { bool active; wdentry_t func; wdparm_t arg; unsigned expiry; };
struct work_s { bool queued; };
struct sdio_dev_s { int unused; };
struct stm32_dev_s
{
  struct sdio_dev_s dev;
  uint32_t *buffer;
  size_t remaining;
  uint32_t receivecnt, xfrmask, waitevents, waitmask;
  struct wdog_s waitwdog, fifowdog;
  struct work_s cbfifo;
  uint8_t rxfifo[64];
};

#define OK 0
#define HPWORK 0
#define DEBUGASSERT(x) ((void)0)
#define FIFO_SIZE_IN_BYTES 64
#define STM32_SDMMC_STA_OFFSET 0
#define STM32_SDMMC_FIFO_OFFSET 1
#define STM32_SDMMC_DCOUNT_OFFSET 2
#define STM32_SDMMC_CMD_OFFSET 3
#define STM32_SDMMC_IDMACTRLR_OFFSET 4
#define STM32_SDMMC_ICR_OFFSET 5
#define STM32_SDMMC_IDMABASE0R_OFFSET 6
#define STM32_SDMMC_MASK_OFFSET 7
#define STM32_SDMMC_STA_RXFIFOE 1u
#define STM32_SDMMC_STA_DPSMACT 2u
#define STM32_SDMMC_STA_RXFIFOHF 4u
#define STM32_SDMMC_STA_TXFIFOHE 8u
#define STM32_SDMMC_STA_DATAEND 16u
#define STM32_SDMMC_STA_DCRCFAIL 32u
#define STM32_SDMMC_STA_DTIMEOUT 64u
#define STM32_SDMMC_STA_RXOVERR 128u
#define STM32_SDMMC_STA_TXUNDERR 256u
#define STM32_SDMMC_RESPDONE_STA 512u
#define STM32_SDMMC_CMDDONE_STA 1024u
#define STM32_SDMMC_MASK_TXFIFOHEIE STM32_SDMMC_STA_TXFIFOHE
#define STM32_SDMMC_CMD_CMDSTOP 4u
#define STM32_SDMMC_CMD_CPSMEN 8u
#define STM32_SDMMC_IDMACTRLR_IDMAEN 1u
#define STM32_SDMMC_XFRDONE_ICR (16u | 32u | 64u | 128u | 256u)
#define STM32_SDMMC_RECV_MASK (STM32_SDMMC_XFRDONE_ICR | STM32_SDMMC_STA_RXFIFOHF)
#define STM32_SDMMC_SEND_MASK (STM32_SDMMC_XFRDONE_ICR | STM32_SDMMC_STA_TXFIFOHE)
#define STM32_SDMMC_WAITALL_ICR (STM32_SDMMC_XFRDONE_ICR | 512u | 1024u)
#define SDIOWAIT_TRANSFERDONE 1u
#define SDIOWAIT_ERROR 2u
#define SDIOWAIT_TIMEOUT 4u
#define SDIOWAIT_RESPONSEDONE 8u
#define SDIOWAIT_CMDDONE 16u
#define WDOG_ISACTIVE(w) ((w)->active)
#define mcerr(...) ((void)0)
#define SDMMC_DTIMER_DATATIMEOUT_MS 100
#define SAMPLENDX_BEFORE_SETUP 0
#define SAMPLENDX_AFTER_SETUP 1
#define SAMPLENDX_END_TRANSFER 2
#define stm32_sampleinit() ((void)0)
#define stm32_sample(p,n) ((void)0)

static unsigned int depth, fifo_reads, available_words, notifications, ticks;
static unsigned int last_event;
static uint32_t status, dcount, idma;
static bool inject, cancel_returned;
static struct stm32_dev_s *pending_cancel;
static uint8_t cancel_image[64];
static uint32_t *observed_buffer;
static int stm32_cancel(struct sdio_dev_s *dev);

static irqstate_t enter_critical_section(void) { return depth++; }
static void leave_critical_section(irqstate_t flags)
{
  depth = flags;
  if (!depth && pending_cancel)
    {
      struct stm32_dev_s *p = pending_cancel;
      pending_cancel = NULL;
      assert(stm32_cancel(&p->dev) == 0);
      memcpy(cancel_image, observed_buffer, sizeof(cancel_image));
      cancel_returned = true;
    }
}
static void test_before_store(struct stm32_dev_s *p)
{
  if (inject)
    {
      inject = false;
      if (depth)
        {
          pending_cancel = p; /* Ordinary IRQ cannot cross critical section. */
        }
      else
        {
          assert(stm32_cancel(&p->dev) == 0);
          memcpy(cancel_image, observed_buffer, sizeof(cancel_image));
          cancel_returned = true;
        }
    }
}
static uint32_t sdmmc_getreg32(struct stm32_dev_s *p, int offset)
{
  if (offset == STM32_SDMMC_FIFO_OFFSET)
    {
      fifo_reads++;
      if (available_words) available_words--;
      return 0x44332211;
    }
  if (offset == STM32_SDMMC_DCOUNT_OFFSET) return dcount;
  if (offset == STM32_SDMMC_MASK_OFFSET) return p->xfrmask | p->waitmask;
  uint32_t value = status | (available_words ? 0 : STM32_SDMMC_STA_RXFIFOE);
  return available_words < 8 ? value & ~STM32_SDMMC_STA_RXFIFOHF : value;
}
static void sdmmc_putreg32(struct stm32_dev_s *p, uint32_t value, int offset)
{
  if (offset == STM32_SDMMC_IDMACTRLR_OFFSET) idma = value;
  if (offset == STM32_SDMMC_ICR_OFFSET) status &= ~value;
  if (offset == STM32_SDMMC_MASK_OFFSET) p->xfrmask = value;
}
static void sdmmc_modifyreg32(struct stm32_dev_s *p, int off, uint32_t clear, uint32_t set) {}
static void stm32_configxfrints(struct stm32_dev_s *p, uint32_t mask) { p->xfrmask = mask; }
static void stm32_configwaitints(struct stm32_dev_s *p, uint32_t mask, unsigned events, unsigned wake) { p->waitevents = events; }
static void stm32_endwait(struct stm32_dev_s *p, unsigned event) { notifications++; last_event = event; p->waitevents = p->waitmask = 0; }
static void stm32_sendfifo(struct stm32_dev_s *p) { assert(!"TX IRQ is outside this RX regression"); }
static void stm32_datadisable(struct stm32_dev_s *p) { idma = 0; }
static void stm32_dataconfig(struct stm32_dev_s *p, unsigned timeout, size_t n, bool receive) { dcount = n; }
static void up_invalidate_dcache(uintptr_t a, uintptr_t b) {}
static int wd_cancel(struct wdog_s *w) { w->active = false; return 0; }
static int wd_start(struct wdog_s *w, unsigned delay, wdentry_t f, wdparm_t a)
{
  assert(delay == 1);
  w->active = true; w->func = f; w->arg = a; w->expiry = ticks + delay;
  return 0;
}
static int work_queue(int queue, struct work_s *w, void (*f)(void *), void *arg, unsigned delay)
{
  w->queued = true;
  return 0;
}

/* DRIVER_FUNCTIONS */

static void monitor(struct stm32_dev_s *p)
{
#if BASELINE
  stm32_sdmmc_fifo_monitor(p);
#else
  stm32_sdmmc_fifo_monitor((wdparm_t)p);
#endif
}
static void init(struct stm32_dev_s *p, uint32_t *buffer, size_t n)
{
  memset(p, 0, sizeof(*p)); memset(buffer, 0xa5, 64);
  depth = fifo_reads = notifications = ticks = last_event = 0;
  available_words = 1; status = STM32_SDMMC_STA_DPSMACT;
  inject = cancel_returned = false; pending_cancel = NULL;
  observed_buffer = buffer;
  assert(stm32_recvsetup(&p->dev, (uint8_t *)buffer, n) == 0);
}
int main(void)
{
  struct stm32_dev_s p;
  uint32_t buffer[16], next[16];
  (void)next;
  init(&p, buffer, 16); inject = true; monitor(&p);
  assert(cancel_returned && !depth);
  bool late_write = memcmp(cancel_image, buffer, sizeof(buffer)) != 0;
#if BASELINE
  assert(late_write);
  puts("BASELINE_REPRODUCED buffer_written_after_cancel_return hardware_NONE");
#else
  assert(!late_write && !p.remaining && !p.receivecnt && !p.buffer);
  assert(!p.fifowdog.active && !idma && !p.xfrmask);
  monitor(&p); assert(!memcmp(cancel_image, buffer, sizeof(buffer)));

  /* Pending monitor cancelled before a new receive/send setup. */
  init(&p, buffer, 64); available_words = 1; monitor(&p);
  assert(p.fifowdog.active && !fifo_reads);
  assert(stm32_cancel(&p.dev) == 0 && !p.fifowdog.active);
  assert(stm32_recvsetup(&p.dev, (uint8_t *)next, 64) == 0);
  assert(!p.fifowdog.active && p.buffer == next && p.remaining == 64);
  monitor(&p); assert(p.fifowdog.active);
  assert(stm32_sendsetup(&p.dev, (uint8_t *)buffer, 64) == 0);
  assert(!p.fifowdog.active && !p.receivecnt);
  monitor(&p); assert(!p.fifowdog.active && p.remaining == 64);

  /* Completion revokes monitor before the single completion notification. */
  init(&p, buffer, 64); monitor(&p); assert(p.fifowdog.active);
  p.waitevents = SDIOWAIT_TRANSFERDONE;
  stm32_endtransfer(&p, SDIOWAIT_TRANSFERDONE);
  assert(!p.fifowdog.active && !p.buffer && !p.remaining && notifications == 1);
  monitor(&p); assert(!p.fifowdog.active && notifications == 1);

  /* Constant nonempty FIFO still consumes at most eight words per monitor. */
  const unsigned sizes[] = {1, 2, 3, 4, 16, 31, 32};
  for (unsigned i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++)
    {
      unsigned n = sizes[i]; init(&p, buffer, n);
      if (n < 32) assert(idma == STM32_SDMMC_IDMACTRLR_IDMAEN);
      else assert(!idma);
      available_words = 100; monitor(&p);
      assert(fifo_reads == (n < 32 ? (n + 3) / 4 : 0));
      assert(fifo_reads <= 8);
      if (n < 32)
        {
          assert(!p.remaining);
          const uint8_t pattern[] = {0x11, 0x22, 0x33, 0x44};
          for (unsigned j = 0; j < n; j++) assert(((uint8_t *)buffer)[j] == pattern[j % 4]);
          for (unsigned j = n; j < 64; j++) assert(((uint8_t *)buffer)[j] == 0xa5);
        }
      assert(stm32_cancel(&p.dev) == 0);
      assert(stm32_cancel(&p.dev) == 0 && !depth && !p.fifowdog.active);
    }

  /* Actual IRQ drains a full 64-byte FIFO before processing DATAEND. A
   * previously armed deadline survives another RX FIFO interrupt.
   */
  init(&p, buffer, 64); available_words = 16;
  status |= STM32_SDMMC_STA_RXFIFOHF | STM32_SDMMC_STA_DATAEND;
  p.waitevents = SDIOWAIT_TRANSFERDONE;
  assert(stm32_sdmmc_interrupt(0, NULL, &p) == 0);
  assert(fifo_reads == 16 && notifications == 1 && last_event == SDIOWAIT_TRANSFERDONE);
  for (unsigned j = 0; j < 16; j++) assert(buffer[j] == 0x44332211);
  assert(!p.fifowdog.active);

  /* 35 byte transfer: 32 via SDMMC IRQ, fractional tail via timer IRQ. */
  init(&p, buffer, 35); available_words = 8;
  status |= STM32_SDMMC_STA_RXFIFOHF;
  assert(stm32_sdmmc_interrupt(0, NULL, &p) == 0);
  assert(p.remaining == 3 && p.fifowdog.active && p.fifowdog.expiry == 1);
  available_words = 1;
  wdentry_t expired = p.fifowdog.func; wdparm_t arg = p.fifowdog.arg;
  p.fifowdog.active = false; expired(arg);
  assert(!p.remaining && fifo_reads == 9 && !depth);
  for (unsigned j = 35; j < 64; j++) assert(((uint8_t *)buffer)[j] == 0xa5);
  status = STM32_SDMMC_STA_DATAEND; p.waitevents = SDIOWAIT_TRANSFERDONE;
  assert(stm32_sdmmc_interrupt(0, NULL, &p) == 0 && notifications == 1);

  init(&p, buffer, 64); available_words = 8;
  p.fifowdog.active = true; p.fifowdog.expiry = 99;
  status |= STM32_SDMMC_STA_RXFIFOHF;
  assert(stm32_sdmmc_interrupt(0, NULL, &p) == 0);
  assert(p.remaining == 32 && p.fifowdog.expiry == 99);
  assert(stm32_cancel(&p.dev) == 0 && !p.fifowdog.active);

  /* Small-transfer internal IDMA buffer is copied by the actual DATAEND
   * IRQ path, not by manually invoking the FIFO monitor.
   */
  for (unsigned n = 1; n < 32; n += 2)
    {
      init(&p, buffer, n); available_words = 0;
      for (unsigned j = 0; j < sizeof(p.rxfifo); j++) p.rxfifo[j] = (uint8_t)j;
      status = STM32_SDMMC_STA_DATAEND; p.waitevents = SDIOWAIT_TRANSFERDONE;
      assert(stm32_sdmmc_interrupt(0, NULL, &p) == 0);
      assert(!fifo_reads && notifications == 1 && last_event == SDIOWAIT_TRANSFERDONE);
      assert(!memcmp(buffer, p.rxfifo, n) && !p.buffer && !p.remaining);
      for (unsigned j = n; j < 64; j++) assert(((uint8_t *)buffer)[j] == 0xa5);
    }
  puts("PIO_CANCEL_PASS race queue_revoke receive_send completion IRQ deadline IDMA_copy tail_bound hardware_NONE");
#endif
  return 0;
}
