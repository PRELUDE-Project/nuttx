/* SPDX-License-Identifier: Apache-2.0 */
/* Extracted Apache NuttX functions with host-only test doubles. */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <errno.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define OK 0
#define MMCSD_CARDTYPE_SDV2 4
#define MMCSD_CARDTYPE_BLOCK 8
#define MMCSD_CMD17 17
#define SDIOWAIT_TRANSFERDONE 1
#define SDIOWAIT_TIMEOUT 2
#define SDIOWAIT_ERROR 4
#define MMCSD_BLOCK_RDATADELAY 100
struct sdio_dev_s{int x;};static struct sdio_dev_s gdev;
struct mmcsd_state_s{struct sdio_dev_s*dev;unsigned type,csd[4],blocksize,blockshift;};
struct scr_lease_key{int dummy;};
static struct mmcsd_state_s g_scrdiag_state;
static bool g_scrdiag_ready,g_sector_attempted;
typedef int irqstate_t;static int depth,stage,calls[6],finish_ret;static uint8_t arena[512];
static int enter_critical_section(void){return depth++;}static void leave_critical_section(int f){assert(depth==f+1);depth--;}
static int stm32_sector_claim(struct sdio_dev_s*d,uint64_t l,uint64_t cap,bool block,uint8_t**a,struct scr_lease_key*k){assert(d==&gdev&&l==0&&cap==1024&&block&&depth==0);calls[0]++;if(stage==1)return -EBUSY;*a=arena;k->dummy=1;return 0;}
#define SDIO_BLOCKSETUP(d,b,n) assert((d)==&gdev&&(b)==512&&(n)==1)
#define SDIO_WAITENABLE(d,e,t) assert((d)==&gdev&&(e)==7&&(t)==100)
static int setup(struct sdio_dev_s*d,uint8_t*b,size_t n){assert(d==&gdev&&b==arena&&n==512);calls[1]++;return stage==2?-ENOTSUP:0;}
#define SDIO_DMARECVSETUP setup
static int mmcsd_sendcmdpoll(struct mmcsd_state_s*p,unsigned c,uint32_t a){assert(p==&g_scrdiag_state&&c==17&&a==0);calls[2]++;return stage==3?-ETIMEDOUT:0;}
static int mmcsd_recv_r1(struct mmcsd_state_s*p,unsigned c){assert(p==&g_scrdiag_state&&c==17);calls[3]++;return stage==4?-EIO:0;}
static int mmcsd_eventwait(struct mmcsd_state_s*p,unsigned e){assert(p==&g_scrdiag_state&&e==6);calls[4]++;return stage==5?-ETIMEDOUT:0;}
static int stm32_sector_finish(struct sdio_dev_s*d,struct scr_lease_key k,int r,uint8_t*out){assert(d==&gdev&&k.dummy==1);calls[5]++;finish_ret=r;if(r==0)memcpy(out,arena,512);return r;}
static int mmcsd_sector_validate(struct mmcsd_state_s *priv, uint64_t lba,
                                 uint64_t *capacity)
{
  if (priv->type != (MMCSD_CARDTYPE_SDV2 | MMCSD_CARDTYPE_BLOCK) ||
      (priv->csd[0] >> 30) != 1 || ((priv->csd[1] >> 16) & 15) != 9 ||
      priv->blocksize != 512 || priv->blockshift != 9)
    return -ENOTSUP;
  uint32_t csize = ((priv->csd[1] & 0x3f) << 16) | (priv->csd[2] >> 16);
  uint64_t blocks = ((uint64_t)csize + 1) << 10;
  if (lba >= blocks || lba > UINT32_MAX) return -ERANGE;
  *capacity = blocks;
  return OK;
}int mmcsd_sector_readonce(struct sdio_dev_s *dev, uint64_t lba,
                           uint8_t *output, size_t output_size)
{
  struct mmcsd_state_s *priv = &g_scrdiag_state;
  struct scr_lease_key key;
  uint8_t *arena;
  uint64_t capacity;
  int ret;
  if (!dev || !output || output_size != 512) return -EINVAL;
  irqstate_t flags = enter_critical_section();
  if (!g_scrdiag_ready || dev != priv->dev || g_sector_attempted)
    { leave_critical_section(flags); return -EBUSY; }
  ret = mmcsd_sector_validate(priv, lba, &capacity);
  if (ret != OK) { leave_critical_section(flags); return ret; }
  g_sector_attempted = true;
  leave_critical_section(flags);
  ret = stm32_sector_claim(dev, lba, capacity, true, &arena, &key);
  if (ret != OK) return ret;
  SDIO_BLOCKSETUP(dev, 512, 1);
  SDIO_WAITENABLE(dev, SDIOWAIT_TRANSFERDONE | SDIOWAIT_TIMEOUT |
                      SDIOWAIT_ERROR, MMCSD_BLOCK_RDATADELAY);
  ret = SDIO_DMARECVSETUP(dev, arena, 512);
  if (ret == OK) ret = mmcsd_sendcmdpoll(priv, MMCSD_CMD17, (uint32_t)lba);
  if (ret == OK) ret = mmcsd_recv_r1(priv, MMCSD_CMD17);
  if (ret == OK) ret = mmcsd_eventwait(priv, SDIOWAIT_TIMEOUT | SDIOWAIT_ERROR);
  /* The finish path seals failures before releasing caller ownership.
   * No generic cancel/reset or retry, even when the transfer failed early.
   */
  return stm32_sector_finish(dev, key, ret, output);
}
static void reset(void){g_scrdiag_state=(struct mmcsd_state_s){.dev=&gdev,.type=12,.csd={1u<<30,9u<<16,0,0},.blocksize=512,.blockshift=9};g_scrdiag_ready=true;g_sector_attempted=false;memset(calls,0,sizeof calls);finish_ret=123;depth=0;}
int main(void){uint8_t out[512];uint64_t cap=99;
for(stage=0;stage<6;stage++){reset();memset(out,0xa5,512);memset(arena,0x5a,512);int r=mmcsd_sector_readonce(&gdev,0,out,512);assert(depth==0&&g_sector_attempted);assert(r==(stage==1?-EBUSY:stage==2?-ENOTSUP:stage==3||stage==5?-ETIMEDOUT:stage==4?-EIO:0));assert(out[0]==(stage?0xa5:0x5a));assert(calls[5]==(stage!=1));if(stage!=1)assert(finish_ret==r);int before=calls[0];assert(mmcsd_sector_readonce(&gdev,0,out,512)==-EBUSY&&calls[0]==before);}
reset();assert(mmcsd_sector_readonce(&gdev,1024,out,512)==-ERANGE&&!g_sector_attempted&&calls[0]==0);
reset();g_scrdiag_ready=false;assert(mmcsd_sector_readonce(&gdev,0,out,512)==-EBUSY&&calls[0]==0);
reset();assert(mmcsd_sector_readonce(&gdev,0,out,511)==-EINVAL&&calls[0]==0);
reset();g_scrdiag_state.csd[0]=2u<<30;assert(mmcsd_sector_validate(&g_scrdiag_state,0,&cap)==-ENOTSUP&&cap==99);
reset();g_scrdiag_state.csd[1]|=0x3f;g_scrdiag_state.csd[2]=0xffff0000;assert(mmcsd_sector_validate(&g_scrdiag_state,UINT32_MAX,&cap)==0&&cap==UINT64_C(4294967296));assert(mmcsd_sector_validate(&g_scrdiag_state,UINT64_C(4294967296),&cap)==-ERANGE);
puts("PASS actual upper validation/readonce control: CSD/type/bounds, output512, all failing stages preserve errno/no premature copy, attempt consumed/no retry. Lower/MMIO mocked; no physical-stop proof.");}
