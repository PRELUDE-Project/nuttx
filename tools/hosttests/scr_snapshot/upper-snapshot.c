
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


#include <stdarg.h>
#include <inttypes.h>
#define FAR
#define CONFIG_DEBUG_FS_ERROR 1
struct mmcsd_state_s{struct sdio_dev_s*dev;};
static int operation,deny_snapshot,logs;
static void ferr(const char*fmt,...){assert(depth==0);logs++;va_list ap;va_start(ap,fmt);vprintf(fmt,ap);va_end(ap);}
int stm32_scr_claim(struct sdio_dev_s*d,uint8_t**a,struct scr_lease_key*k){struct stm32_dev_s*p=(void*)d;*a=p->scrlease.arena;return scr_lease_begin(&p->scrlease,owner,*a,k);}
static int mmcsd_get_scr_data(struct mmcsd_state_s*p,uint32_t*a){(void)p;(void)a;if(deny_snapshot)owner=8;return operation;}
int stm32_scr_finish(struct sdio_dev_s*d,struct scr_lease_key k,int ret,uint32_t out[2]){(void)d;(void)k;(void)out;assert(depth==0);return ret;}
int stm32_scr_stop_status(struct sdio_dev_s*d,struct scr_lease_key k){(void)d;(void)k;return -EINPROGRESS;}
static int mmcsd_get_scr(FAR struct mmcsd_state_s *priv, uint32_t scr[2])
{
  struct scr_lease_key key;
  uint8_t *arena;
  int ret;
  /* Caller pointer is used only by task-side qualified final copy. */
  ret = stm32_scr_claim(priv->dev, &arena, &key);
  if (ret != OK) return ret;
  ret = mmcsd_get_scr_data(priv, (uint32_t *)arena);
#ifdef CONFIG_DEBUG_FS_ERROR
  /* Trusted stack storage; capture before finish revokes the lease. */
  struct scr_hw_snapshot observation;
  int observed = stm32_scr_snapshot(priv->dev, key, &observation);
#endif
  ret = stm32_scr_finish(priv->dev, key, ret, scr);
#ifdef CONFIG_DEBUG_FS_ERROR
  if (observed == OK)
    {
      ferr("SCR pre-finish observation v%lu gen=%" PRIu64 ": state=%lu "
           "STA=%08lx/%08lx MASK=%08lx "
           "IDMA=%08lx DLEN=%lu DCTRL=%08lx; not stop proof\n",
           (unsigned long)observation.version, observation.generation,
           (unsigned long)observation.state,
           (unsigned long)observation.sta_before,
           (unsigned long)observation.sta_after,
           (unsigned long)observation.mask,
           (unsigned long)observation.idmactrl,
           (unsigned long)observation.dlen,
           (unsigned long)observation.dctrl);
    }
  else
    {
      ferr("SCR observation unavailable: %d\n", observed);
    }
#endif
  ferr("SCR review: result=%d, stop_status=%d (diagnostic only)\n",
       ret, stm32_scr_stop_status(priv->dev, key));
  return ret;
}
int main(void){for(int fail=0;fail<2;fail++)for(int denied=0;denied<2;denied++){
memset(&g_sdmmcdev1,0,sizeof g_sdmmcdev1);owner=7;irq=false;reads=logs=0;operation=fail?-EIO:0;deny_snapshot=denied;struct mmcsd_state_s p={&g_sdmmcdev1.dev};uint32_t out[2]={123,456};assert(mmcsd_get_scr(&p,out)==operation);assert(reads==(denied?0:6));assert(logs==2&&!depth&&out[0]==123&&out[1]==456);}
puts("PASS actual upper wrapper + snapshot: capture before finish, logs outside critical section, denied snapshot cannot override SCR result. Transfer/finish are mocked.");}
