/* SPDX-License-Identifier: Apache-2.0 */
/* Host model for exact extracted driver functions; not an H753 simulation. */
#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#define FAR
#define SDIOWAIT_TRANSFERDONE (1u<<2)
#define SDIOWAIT_TIMEOUT (1u<<3)
#define SDIOWAIT_ERROR (1u<<4)
#define SDIOWAIT_WRCOMPLETE (1u<<5)
#define HAVE_SDMMC_SDIO_MODE
#define GPIO_PORT_MASK 1
#define GPIO_PIN_MASK 2
#define GPIO_PUPD_MASK 4
#define GPIO_INPUT 8
#define GPIO_EXTI 16
#define SCR_LEASE_HANDOFF 2
#define SCR_LEASE_ACTIVE 1
#define STM32_SDMMC_MASK_OFFSET 8
#define OK 0
#define DEBUGASSERT(c) assert(c)
typedef unsigned irqstate_t;
typedef unsigned sdio_eventset_t;
struct sdio_dev_s { sdio_eventset_t (*eventwait)(struct sdio_dev_s *); };
struct stm32_dev_s {
 struct sdio_dev_s dev;
 struct {unsigned state; uintptr_t owner;} scrlease;
 unsigned waitevents,wkupevent,xfrmask,waitmask,sdiointmask,saved_mask,waitsem;
 bool sdiomode,waitwdog; int d0_gpio;
};
struct mmcsd_state_s {struct sdio_dev_s *dev;};
static unsigned depth,polls,seals,cancels,dumps,writes,errors;
static int sem_rc;
static irqstate_t enter_critical_section(void){return depth++;}
static void leave_critical_section(irqstate_t f){assert(depth==f+1);depth=f;}
static void sdmmc_putreg32(struct stm32_dev_s*p,uint32_t v,unsigned off){assert(depth&&off==STM32_SDMMC_MASK_OFFSET);p->saved_mask=v;++writes;}
static void stm32_dumpsamples(struct stm32_dev_s*p){(void)p;assert(!depth);++dumps;}
static int nxsem_wait_uninterruptible(unsigned*p){assert(depth);++polls;if(sem_rc)return sem_rc;assert(*p);--*p;return 0;}
static void wd_cancel(bool*b){assert(depth);++cancels;*b=false;}
static void ferr(const char*f,unsigned e){assert(f&&e);++errors;}
#define SDIO_EVENTWAIT(d) ((d)->eventwait(d))
#ifdef CONFIG_SD_SECTOR_READ_REVIEW
static int nxsched_gettid(void){return 1;}
static void scr_seal(struct stm32_dev_s*p,int e){(void)p;assert(depth&&e==-EIO);++seals;}
#endif
#ifdef CONFIG_MMCSD_SDIOWAIT_WRCOMPLETE
static int stm32_sdmmc_rdyinterrupt(int a,void*b,void*c){(void)a;(void)b;(void)c;assert(false);return 0;}
static void stm32_gpiosetevent(int pin,bool r,bool f,bool e,int(*h)(int,void*,void*),void*p){(void)pin;(void)r;(void)f;(void)e;(void)h;(void)p;assert(false);}
static bool stm32_gpioread(int a){(void)a;assert(false);return false;}
static void stm32_endwait(struct stm32_dev_s*p,sdio_eventset_t e){(void)p;(void)e;assert(false);}
#endif
#include "actual.inc"
static void run(unsigned wait,unsigned wake,int rc,int expected,unsigned wp,unsigned wc,unsigned we){
 for(unsigned sdio=0;sdio<2;sdio++){
  polls=cancels=seals=dumps=writes=errors=0;sem_rc=rc;
  struct stm32_dev_s p={.dev={stm32_eventwait},.waitevents=wait,.wkupevent=wake,.waitmask=0x100u,
      .xfrmask=0x200u,.sdiointmask=0x400u,.sdiomode=sdio,.waitsem=wake!=0,.waitwdog=wait!=0};
  struct mmcsd_state_s m={&p.dev};
  assert(mmcsd_eventwait(&m,SDIOWAIT_TIMEOUT|SDIOWAIT_ERROR)==expected);
  assert(!depth&&!p.waitevents&&!p.wkupevent&&!p.waitmask);
  assert(p.saved_mask==(sdio?0x600u:0x200u));
  assert(polls==wp&&cancels==wc&&dumps==1&&writes==1&&errors==we);
#ifdef CONFIG_SD_SECTOR_READ_REVIEW
  assert(seals==wc);
#else
  assert(!seals);
#endif
  if(wc)assert(!p.waitwdog);
 }
}
int main(void){
 run(0,SDIOWAIT_TRANSFERDONE,0,0,1,0,0);
 run(0,SDIOWAIT_TRANSFERDONE|SDIOWAIT_TIMEOUT|SDIOWAIT_ERROR,0,-ETIMEDOUT,1,0,1);
 run(SDIOWAIT_TRANSFERDONE,0,-ECANCELED,-EIO,1,1,1);
#ifdef CONFIG_MMCSD_SDIOWAIT_WRCOMPLETE
#ifdef CONFIG_SD_RECEIVE_LEASE_ONLY_REVIEW
 run(0,0,0,-EIO,0,0,0);
#else
 run(0,0,0,0,0,0,0);
#endif
#endif
 puts("PASS eventwait source: pre-posted transfer, timeout precedence, task cancel, and masked exit");
}
