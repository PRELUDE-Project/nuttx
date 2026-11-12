/* SPDX-License-Identifier: Apache-2.0 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#define OK 0
#define MMCSD_SCR_BUSWIDTH_1BIT 1
#define IS_SD(t) ((t)==1)
typedef int irqstate_t;static int depth;
struct sdio_dev_s{int x;};static struct sdio_dev_s dev;
struct mmcsd_state_s{struct sdio_dev_s*dev;int caps,buswidth,type;};
static int initcalls,presentcalls,attachcalls,identifycalls,scrcalls,fail;
static int enter_critical_section(void){return depth++;}
static void leave_critical_section(int f){assert(depth==f+1);depth--;}
static struct sdio_dev_s*sdio_initialize(int s){assert(depth==0&&s==0);initcalls++;return fail==1?NULL:&dev;}
static void sdio_mediachange(struct sdio_dev_s*d,bool b){assert(d==&dev&&b&&depth==0);presentcalls++;}
static int caps(struct sdio_dev_s*d){assert(d==&dev);return 1;}
static int attach(struct sdio_dev_s*d){assert(d==&dev);attachcalls++;return fail==2?-EIO:0;}
#define SDIO_CAPABILITIES caps
#define SDIO_ATTACH attach
static int mmcsd_cardidentify(struct mmcsd_state_s*s){identifycalls++;s->type=fail==4?0:1;return fail==3?-ETIMEDOUT:0;}
static int mmcsd_sdinitialize(struct mmcsd_state_s*s){assert(s->type==1);scrcalls++;return fail==5?-EIO:0;}
int mmcsd_scr_diagnostic(struct sdio_dev_s *dev)
{
  static struct mmcsd_state_s state;
  static bool started;
  irqstate_t flags = enter_critical_section();
  if (started || dev == NULL)
    {
      leave_critical_section(flags);
      return dev == NULL ? -EINVAL : -EBUSY;
    }
  started = true;
  leave_critical_section(flags);
  state.dev = dev;
  state.caps = SDIO_CAPABILITIES(dev);
  state.buswidth = MMCSD_SCR_BUSWIDTH_1BIT;
  int ret = SDIO_ATTACH(dev);
  if (ret != OK) return ret;
  ret = mmcsd_cardidentify(&state);
  if (ret != OK) return ret;
  if (!IS_SD(state.type)) return -ENOTSUP;
  return mmcsd_sdinitialize(&state);
}/* SPDX-License-Identifier: Apache-2.0 */
/* H753 board-adapter hooks; not portable to a different SDIO controller. */
int scrdiag_main(int argc, char **argv)
{
  static bool started;
  (void)argv;
  if (argc != 1) return 1;
  irqstate_t f = enter_critical_section();
  if (started) { leave_critical_section(f); return 1; }
  started = true;
  leave_critical_section(f);
  struct sdio_dev_s *dev = sdio_initialize(0);
  if (dev == NULL) return 1;
  /* Manual invocation assumes the operator verified the physical card.
   * No callback is registered; never claim an actually detected card.
   */
  sdio_mediachange(dev, true);
  int ret = mmcsd_scr_diagnostic(dev);
  printf("SCR-only diagnostic result %d; no sector test or stop proof; no retry\n", ret);
  return ret == 0 ? 0 : 1;
}
int main(int argc,char**argv){assert(argc==2);fail=atoi(argv[1]);assert(scrdiag_main(2,NULL)==1&&initcalls==0);int r=scrdiag_main(1,NULL);assert(r==(fail!=0));assert(initcalls==1&&depth==0);int pc=presentcalls,ac=attachcalls,ic=identifycalls,sc=scrcalls;assert(scrdiag_main(1,NULL)==1);assert(initcalls==1&&pc==presentcalls&&ac==attachcalls&&ic==identifycalls&&sc==scrcalls);if(fail!=1)assert(mmcsd_scr_diagnostic(&dev)==-EBUSY);puts("PASS actual manual entry + upper entry: each failure terminal, repeat rejected before initialization, no block/probe registration in extracted route; SD/MMIO work mocked.");}
