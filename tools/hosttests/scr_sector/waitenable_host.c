/* SPDX-License-Identifier: Apache-2.0 */

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <errno.h>
#define CONFIG_SD_SCR_CONTAINMENT_REVIEW 1
#define HAVE_SDMMC_SDIO_MODE 1
#define SDIOWAIT_CMDDONE (1u << 0)
#define SDIOWAIT_RESPONSEDONE (1u << 1)
#define SDIOWAIT_TRANSFERDONE (1u << 2)
#define SDIOWAIT_TIMEOUT (1u << 3)
#define SDIOWAIT_ERROR (1u << 4)
/* The register and timer model is local RAM, not an H753 simulation. */
#define STM32_SDMMC_CMDDONE_MASK (1u << 7)
#define STM32_SDMMC_RESPDONE_MASK (1u << 8)
#define STM32_SDMMC_XFRDONE_MASK 0u
#define STM32_SDMMC_WAITALL_ICR 3u
#define STM32_SDMMC_ICR_OFFSET 4u
#define STM32_SDMMC_MASK_OFFSET 8u
#define MSEC2TICK(ms) (((ms) + 9u) / 10u)
#define OK 0
#define SCR_LEASE_ACTIVE 1
#define DEBUGASSERT(c) assert(c)
typedef unsigned irqstate_t;
typedef unsigned sdio_eventset_t;
typedef uintptr_t wdparm_t;
struct sdio_dev_s { unsigned unused; };
struct stm32_dev_s {
  struct sdio_dev_s dev;
  struct { unsigned state; } scrlease;
  unsigned scr_phase;
  bool scr_completed;
  sdio_eventset_t waitevents, wkupevent;
  bool waitwdog, sdiomode;
  uint32_t xfrmask, waitmask, sdiointmask, saved_mask;
};
static unsigned depth, starts, posts, seals, logs, mask_writes;
static int seal_reason, wd_result, seen_delay;
static bool inject_latched;
static irqstate_t enter_critical_section(void) { return depth++; }
static void leave_critical_section(irqstate_t f) { assert(depth == f + 1); depth = f; }
static bool scr_denied(struct stm32_dev_s *p) { (void)p; return false; }
static void sdmmc_putreg32(struct stm32_dev_s *p,uint32_t v,unsigned off) {
  assert(depth);
  if (off == STM32_SDMMC_MASK_OFFSET) {
    assert(depth >= 1);
    p->saved_mask = v;
    ++mask_writes;
  } else {
    assert(off == STM32_SDMMC_ICR_OFFSET && v == STM32_SDMMC_WAITALL_ICR);
  }
}
static void stm32_eventtimeout(wdparm_t p) { (void)p; assert(false); }
static int wd_start(bool *wd,int delay,void(*f)(wdparm_t),wdparm_t p) {
  assert(depth && f == stm32_eventtimeout && p);
  ++starts; seen_delay = delay; *wd = wd_result == 0; return wd_result;
}
static void stm32_endwait(struct stm32_dev_s *p,sdio_eventset_t events) {
  assert(depth); assert(p->wkupevent == 0);
  ++posts; p->waitevents = 0; p->wkupevent = events; p->waitwdog = false;
}
static void scr_seal(struct stm32_dev_s *p,int rc) {
  assert(depth); ++seals; seal_reason = rc;
  /* Direct synthetic injection before the second terminal check.
   * This invokes no actual interrupt and does not establish race reachability.
   */
  if (inject_latched) p->wkupevent = SDIOWAIT_ERROR;
}
static void mcerr(const char *fmt, int err) { assert(fmt && err < 0); ++logs; }
#include "waitenable.inc"
static void clear(void) {
  assert(depth == 0); starts = posts = seals = logs = mask_writes = 0;
  inject_latched = false; wd_result = seen_delay = seal_reason = 0;
}
int main(void) {
  struct stm32_dev_s p = {0};
  clear(); wd_result = -EAGAIN;
  stm32_waitenable(&p.dev,SDIOWAIT_TIMEOUT|SDIOWAIT_TRANSFERDONE,100);
  assert(depth == 0 && starts == 1 && seen_delay == 10 && seals == 1);
  assert(seal_reason == -EAGAIN && posts == 1 && logs == 1);
  assert(p.wkupevent == SDIOWAIT_ERROR && p.waitevents == 0 && !p.waitwdog);
  puts("PASS injected watchdog-admission failure: one seal and ERROR post");
  p = (struct stm32_dev_s){0}; clear();
  stm32_waitenable(&p.dev,SDIOWAIT_TIMEOUT|SDIOWAIT_TRANSFERDONE,0);
  assert(depth == 0 && starts == 0 && seals == 1 && seal_reason == -ETIMEDOUT);
  assert(posts == 1 && p.wkupevent == SDIOWAIT_TIMEOUT && p.waitevents == 0);
  puts("PASS zero timeout: one TIMEOUT post, no watchdog");
  p = (struct stm32_dev_s){0}; clear(); inject_latched = true;
  stm32_waitenable(&p.dev,SDIOWAIT_TIMEOUT|SDIOWAIT_TRANSFERDONE,0);
  assert(depth == 0 && starts == 0 && seals == 1 && seal_reason == -ETIMEDOUT);
  assert(posts == 0 && p.wkupevent == SDIOWAIT_ERROR);
  puts("PASS synthetic already-latched terminal: no new post, latch preserved");
  for (unsigned sdio = 0; sdio < 2; ++sdio) {
    clear();
    p = (struct stm32_dev_s){.xfrmask = 0x20u, .sdiointmask = 0x40u,
                            .sdiomode = sdio != 0};
    const uint32_t keep = 0x20u | (sdio ? 0x40u : 0u);
    stm32_waitenable(&p.dev, SDIOWAIT_CMDDONE | SDIOWAIT_TIMEOUT, 100);
    assert(depth == 0 && starts == 1 && p.waitwdog && posts == 0 && seals == 0);
    assert(p.waitmask == STM32_SDMMC_CMDDONE_MASK && mask_writes == 2);
    assert(p.saved_mask == (keep | STM32_SDMMC_CMDDONE_MASK));
    stm32_configwaitints(&p, 0, 0, SDIOWAIT_CMDDONE);
    assert(depth == 0 && mask_writes == 3 && p.saved_mask == keep);
    assert(p.waitmask == 0 && p.waitevents == 0 && p.wkupevent == SDIOWAIT_CMDDONE);
  }
  puts("PASS actual wait mask adds command mask and removes it without dropping transfer/SDIO masks");
  return 0;
}
