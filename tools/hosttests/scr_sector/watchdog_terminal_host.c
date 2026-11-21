/* SPDX-License-Identifier: Apache-2.0 */

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <stddef.h>
#include <stdio.h>
#include <errno.h>
#define CONFIG_SD_SCR_CONTAINMENT_REVIEW 1
#define CONFIG_SD_SECTOR_READ_REVIEW 1
#define SDIOWAIT_TRANSFERDONE (1u << 2)
#define SDIOWAIT_TIMEOUT (1u << 3)
#define SDIOWAIT_ERROR (1u << 4)
#define STM32_SDMMC_STA_OFFSET 1u
#define STM32_SDMMC_MASK_OFFSET 2u
#define DEBUGASSERT(c) assert(c)
typedef unsigned sdio_eventset_t;
typedef uintptr_t wdparm_t;
enum lease_state { SCR_LEASE_IDLE, SCR_LEASE_ACTIVE,
                   SCR_LEASE_QUARANTINED, SCR_LEASE_RETIRED, SCR_LEASE_HANDOFF };
struct stm32_dev_s {
  struct { enum lease_state state; } scrlease;
  sdio_eventset_t waitevents, wkupevent;
  bool scr_completed, waitwdog;
  unsigned waitsem;
  size_t remaining;
};
static unsigned seals, closes, cancels, configs, reads, info, errors;
static int reason;
static void mcinfo(const char *fmt, uint32_t a, uint32_t b)
{ assert(fmt && a == STM32_SDMMC_STA_OFFSET && b == STM32_SDMMC_MASK_OFFSET); info++; }
static void mcerr(const char *fmt, size_t n) { (void)n; assert(fmt); errors++; }
static uint32_t sdmmc_getreg32(struct stm32_dev_s *p, unsigned offset)
{ assert(p); reads++; return offset; }
static void scr_seal(struct stm32_dev_s *p, int e)
{ assert(p->scrlease.state == SCR_LEASE_ACTIVE); seals++; reason=e; p->scrlease.state=SCR_LEASE_QUARANTINED; }
static void scr_close_producers(struct stm32_dev_s *p) { assert(p); closes++; }
static int wd_cancel(bool *p) { assert(p); *p=false; cancels++; return 0; }
static void stm32_configwaitints(struct stm32_dev_s *p, uint32_t mask,
                                sdio_eventset_t wait, sdio_eventset_t wake)
{ assert(mask == 0); p->waitevents=wait; p->wkupevent=wake; configs++; }
static int nxsem_post(unsigned *n) { ++*n; return 0; }
static void stm32_endwait(struct stm32_dev_s *p, sdio_eventset_t e);
#include "functions.inc"
static void clear(void) { seals=closes=cancels=configs=reads=info=errors=0; reason=0; }
int main(void)
{
  struct stm32_dev_s a={.scrlease={SCR_LEASE_ACTIVE},.waitevents=SDIOWAIT_TIMEOUT,.waitwdog=true};
  clear();
  stm32_eventtimeout((wdparm_t)&a);
  assert(a.scrlease.state==SCR_LEASE_QUARANTINED && reason==-ETIMEDOUT && seals==1);
  assert(a.wkupevent==SDIOWAIT_TIMEOUT && a.waitevents==0 && a.waitsem==1 && !a.waitwdog);
  assert(closes==0 && cancels==1 && configs==1 && reads==2 && info==1 && errors==1);
  stm32_eventtimeout((wdparm_t)&a);
  assert(a.waitsem==1 && seals==1 && cancels==1 && configs==1 && reads==2 && errors==1);
  puts("PASS active timeout posts once, then sequential late callback returns early");
  static const enum lease_state closed[] = {SCR_LEASE_HANDOFF,SCR_LEASE_QUARANTINED,SCR_LEASE_RETIRED};
  for (unsigned i=0;i<sizeof(closed)/sizeof(closed[0]);i++) {
    struct stm32_dev_s p={.scrlease={closed[i]},.waitevents=SDIOWAIT_TIMEOUT,.wkupevent=SDIOWAIT_ERROR,.waitsem=1};
    clear(); stm32_eventtimeout((wdparm_t)&p);
    assert(p.scrlease.state==closed[i] && p.waitevents==SDIOWAIT_TIMEOUT && p.wkupevent==SDIOWAIT_ERROR && p.waitsem==1);
    assert(seals==0 && closes==0 && cancels==0 && configs==0 && reads==0 && info==0 && errors==0);
  }
  puts("PASS HANDOFF/QUARANTINED/RETIRED preserve prior result without status reads or posts");
  struct stm32_dev_s idle={.scrlease={SCR_LEASE_IDLE},.wkupevent=SDIOWAIT_TRANSFERDONE,.waitsem=1};
  clear(); stm32_eventtimeout((wdparm_t)&idle);
  assert(idle.wkupevent==SDIOWAIT_TRANSFERDONE && idle.waitsem==1);
  assert(seals==0 && closes==0 && cancels==0 && configs==0 && reads==2 && info==1 && errors==0);
  puts("PASS idle, prior transfer result without TIMEOUT interest: no new post");
}
