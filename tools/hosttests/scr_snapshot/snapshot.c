
#include <nuttx/scr_lease_review.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define CONFIG_STM32H7_SDMMC1 1
#define CONFIG_STM32H7_SDMMC2 1
#define OK 0
#define STM32_SDMMC_STA_OFFSET 0
#define STM32_SDMMC_MASK_OFFSET 1
#define STM32_SDMMC_IDMACTRLR_OFFSET 2
#define STM32_SDMMC_DLEN_OFFSET 3
#define STM32_SDMMC_DCTRL_OFFSET 4
struct sdio_dev_s {int x;};
struct stm32_dev_s {struct sdio_dev_s dev;struct scr_lease_review scrlease;bool scrinitializing;};
struct stm32_dev_s g_sdmmcdev1,g_sdmmcdev2;
static bool irq;static uintptr_t owner=7;static int depth,reads;typedef int irqstate_t;
static bool up_interrupt_context(void){return irq;}
static uintptr_t nxsched_gettid(void){return owner;}
static irqstate_t enter_critical_section(void){return depth++;}
static void leave_critical_section(int f){assert(depth==f+1);depth=f;}
static uint32_t sdmmc_getreg32(struct stm32_dev_s*p,int off){assert(p==&g_sdmmcdev1||p==&g_sdmmcdev2);assert(depth==1);assert(off>=0&&off<=4);return ++reads;}

/* Reject output aliases into either controller before copying observations. */
static bool scr_snapshot_overlap(uintptr_t out, uintptr_t base)
{
  return out <= base + sizeof(struct stm32_dev_s) - 1 &&
         base <= out + sizeof(struct scr_hw_snapshot) - 1;
}
int stm32_scr_snapshot(struct sdio_dev_s *dev, struct scr_lease_key key,
                       struct scr_hw_snapshot *out)
{
  struct stm32_dev_s *priv = NULL;
  struct scr_hw_snapshot value = {0};
  uintptr_t dest = (uintptr_t)out;
  irqstate_t flags;
  int ret = -ESTALE;
  if (out == NULL || dest % _Alignof(struct scr_hw_snapshot) != 0 ||
      dest > UINTPTR_MAX - sizeof(*out) + 1)
    return -EINVAL;
  if (up_interrupt_context()) return -EPERM;
#ifdef CONFIG_STM32H7_SDMMC1
  if (dev == &g_sdmmcdev1.dev) priv = &g_sdmmcdev1;
  if (scr_snapshot_overlap(dest, (uintptr_t)&g_sdmmcdev1)) return -EINVAL;
#endif
#ifdef CONFIG_STM32H7_SDMMC2
  if (dev == &g_sdmmcdev2.dev) priv = &g_sdmmcdev2;
  if (scr_snapshot_overlap(dest, (uintptr_t)&g_sdmmcdev2)) return -EINVAL;
#endif
  if (priv == NULL) return -ENODEV;
  flags = enter_critical_section();
  if (scr_lease_matches(&priv->scrlease, key) &&
      key.owner == (uintptr_t)nxsched_gettid() && !priv->scrinitializing)
    {
      value.version = 1;
      value.state = priv->scrlease.state;
      value.generation = priv->scrlease.generation;
      value.sta_before = sdmmc_getreg32(priv, STM32_SDMMC_STA_OFFSET);
      value.mask = sdmmc_getreg32(priv, STM32_SDMMC_MASK_OFFSET);
      value.idmactrl = sdmmc_getreg32(priv, STM32_SDMMC_IDMACTRLR_OFFSET);
      value.dlen = sdmmc_getreg32(priv, STM32_SDMMC_DLEN_OFFSET);
      value.dctrl = sdmmc_getreg32(priv, STM32_SDMMC_DCTRL_OFFSET);
      value.sta_after = sdmmc_getreg32(priv, STM32_SDMMC_STA_OFFSET);
      ret = OK;
    }
  leave_critical_section(flags);
  if (ret == OK) *out = value;
  return ret;
}

int main(void){struct scr_lease_key k;struct scr_hw_snapshot out,before;struct stm32_dev_s *p=&g_sdmmcdev1;
assert(scr_lease_begin(&p->scrlease,7,p->scrlease.arena,&k)==0);
for(int state=1;state<=3;state++){p->scrlease.state=state;struct stm32_dev_s saved=*p;reads=0;assert(stm32_scr_snapshot(&p->dev,k,&out)==0);assert(reads==6&&out.sta_before==1&&out.sta_after==6&&out.state==(unsigned)state);assert(!memcmp(&saved,p,sizeof saved)&&!depth);}
before=out;reads=0;
assert(stm32_scr_snapshot(NULL,k,&out)==-ENODEV);
assert(stm32_scr_snapshot(&p->dev,k,NULL)==-EINVAL);
assert(stm32_scr_snapshot(&p->dev,k,(void*)UINTPTR_MAX)==-EINVAL);
assert(stm32_scr_snapshot(&p->dev,k,(void*)&p->scrlease)==-EINVAL);
assert(stm32_scr_snapshot(&p->dev,k,(void*)&g_sdmmcdev2)==-EINVAL);
irq=true;assert(stm32_scr_snapshot(&p->dev,k,&out)==-EPERM);irq=false;
owner=8;assert(stm32_scr_snapshot(&p->dev,k,&out)==-ESTALE);owner=7;
p->scrinitializing=true;assert(stm32_scr_snapshot(&p->dev,k,&out)==-ESTALE);p->scrinitializing=false;
k.generation++;assert(stm32_scr_snapshot(&p->dev,k,&out)==-ESTALE);
assert(!reads&&!depth&&!memcmp(&before,&out,sizeof out));
puts("PASS snapshot current code: six allowed MMIO reads only, changing STA preserved, no device mutation, denied contexts/instance/owner/generation/output aliases leave output untouched. Host model only.");}
