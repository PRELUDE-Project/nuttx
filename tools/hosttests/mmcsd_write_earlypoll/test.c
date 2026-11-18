/* SPDX-License-Identifier: Apache-2.0 */
/* Extracted Apache NuttX functions with host-only transport doubles. */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <sys/types.h>
#include <errno.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
#define FAR
#define OK 0
#define finfo(...) ((void)0)
#define ferr(...) ((void)0)
#define DEBUGASSERT assert
#define SDIO_CAPS_DMASUPPORTED 1
#define SDIO_CAPS_DMABEFOREWRITE 2
#define MMC_CMD6_MODE_WRITE_BYTE 0
#define EXT_CSD_PART_CONF 0
#define MMC_CMD6_MODE(x) (x)
#define MMC_CMD6_INDEX(x) (x)
#define MMC_CMD6_VALUE(x) (x)
#define MMCSD_CMD24 24
#define SDIOWAIT_TRANSFERDONE 1
#define SDIOWAIT_TIMEOUT 2
#define SDIOWAIT_ERROR 4
#define SDIOWAIT_WRCOMPLETE 8
#define MMCSD_BLOCK_WDATADELAY 100
#define IS_BLOCK(t) ((t)!=0)
struct mmcsd_state_s;
struct mmcsd_part_s{struct mmcsd_state_s*priv;};
struct mmcsd_state_s{struct mmcsd_part_s part[1];unsigned partnum;int caps,type,blocksize,blockshift;void*dev;bool wrbusy;};
static int trace[64],count,failstep,errcode;
static int hit(int n){trace[count++]=n;assert(count<64);return failstep==n?errcode:0;}
static int mmcsd_wrprotected(void*p){(void)p;return hit(1);}
static int mmcsd_switch(void*p,int x){(void)p;(void)x;return hit(2);}
#define SDIO_DMAPREFLIGHT(d,b,n) hit(3)
static int mmcsd_transferready(void*p){(void)p;return hit(4);}
static int mmcsd_setblocklen(void*p,int n){(void)p;assert(n==512);return hit(5);}
static int mmcsd_sendcmdpoll(struct mmcsd_state_s*p,int c,off_t a){assert(c==24&&a==(p->type?7:7*512));return hit(6);}
static int mmcsd_recv_r1(void*p,int c){(void)p;assert(c==24);return hit(7);}
#define SDIO_BLOCKSETUP(d,b,n) ((void)hit(8))
#define SDIO_WAITENABLE(d,e,t) ((void)hit(9))
#define SDIO_DMASENDSETUP(d,b,n) hit(10)
#define SDIO_SENDSETUP(d,b,n) ((void)hit(11))
#define SDIO_CANCEL(d) ((void)hit(12))
static int mmcsd_eventwait(void*p,int e){(void)p;assert(e==6);return hit(13);}
static ssize_t baseline_writesingle(FAR struct mmcsd_part_s *part,
                                 FAR const uint8_t *buffer, off_t startblock)
{
  FAR struct mmcsd_state_s *priv = part->priv;
  uint32_t partnum = part - priv->part;
  off_t offset;
  int ret;

  finfo("startblock=%jd\n", (intmax_t)startblock);
  DEBUGASSERT(priv != NULL && buffer != NULL);

  /* Check if the card is locked or write protected (either via software or
   * via the mechanical write protect on the card)
   */

  if (mmcsd_wrprotected(priv))
    {
      ferr("ERROR: Card is locked or write protected\n");
      return -EPERM;
    }

  if (priv->partnum != partnum)
    {
      ret = mmcsd_switch(priv, MMC_CMD6_MODE(MMC_CMD6_MODE_WRITE_BYTE) |
                               MMC_CMD6_INDEX(EXT_CSD_PART_CONF) |
                               MMC_CMD6_VALUE(partnum));
      if (ret != OK)
        {
          ferr("ERROR: mmcsd_switch failed: %d\n", ret);
          return ret;
        }

      priv->partnum = partnum;
    }

#if defined(CONFIG_SDIO_DMA) && defined(CONFIG_ARCH_HAVE_SDIO_PREFLIGHT)
  /* If we think we are going to perform a DMA transfer, make sure that we
   * will be able to before we commit the card to the operation.
   */

  if ((priv->caps & SDIO_CAPS_DMASUPPORTED) != 0)
    {
      ret = SDIO_DMAPREFLIGHT(priv->dev, buffer, priv->blocksize);

      if (ret != OK)
        {
          return ret;
        }
    }
#endif

  /* Verify that the card is ready for the transfer.  The card may still be
   * busy from the preceding write transfer.  It would be simpler to check
   * for write busy at the end of each write, rather than at the beginning of
   * each read AND write, but putting the busy-wait at the beginning of the
   * transfer allows for more overlap and, hopefully, better performance
   */

  ret = mmcsd_transferready(priv);
  if (ret != OK)
    {
      ferr("ERROR: Card not ready: %d\n", ret);
      return ret;
    }

  /* If this is a byte addressed SD card, then convert sector start sector
   * number to a byte offset
   */

  if (IS_BLOCK(priv->type))
    {
      offset = startblock;
    }
  else
    {
      offset = startblock << priv->blockshift;
    }

  finfo("offset=%jd\n", (intmax_t)offset);

  /* Select the block size for the card */

  ret = mmcsd_setblocklen(priv, priv->blocksize);
  if (ret != OK)
    {
      ferr("ERROR: mmcsd_setblocklen failed: %d\n", ret);
      return ret;
    }

  /* If Controller does not need DMA setup before the write then send CMD24
   * now.
   */

  if ((priv->caps & SDIO_CAPS_DMABEFOREWRITE) == 0)
    {
      /* Send CMD24, WRITE_BLOCK, and verify good R1 status is returned */

      mmcsd_sendcmdpoll(priv, MMCSD_CMD24, offset);
      ret = mmcsd_recv_r1(priv, MMCSD_CMD24);
      if (ret != OK)
        {
          ferr("ERROR: mmcsd_recv_r1 for CMD24 failed: %d\n", ret);
          return ret;
        }
    }

  /* Configure SDIO controller hardware for the write transfer */

  SDIO_BLOCKSETUP(priv->dev, priv->blocksize, 1);
  SDIO_WAITENABLE(priv->dev,
                  SDIOWAIT_TRANSFERDONE | SDIOWAIT_TIMEOUT | SDIOWAIT_ERROR,
                  MMCSD_BLOCK_WDATADELAY);

#ifdef CONFIG_SDIO_DMA
  if ((priv->caps & SDIO_CAPS_DMASUPPORTED) != 0)
    {
      ret = SDIO_DMASENDSETUP(priv->dev, buffer, priv->blocksize);
      if (ret != OK)
        {
          finfo("SDIO_DMASENDSETUP: error %d\n", ret);
          SDIO_CANCEL(priv->dev);
          return ret;
        }
    }
  else
#endif
    {
      SDIO_SENDSETUP(priv->dev, buffer, priv->blocksize);
    }

  /* If Controller needs DMA setup before write then only send CMD24 now. */

  if ((priv->caps & SDIO_CAPS_DMABEFOREWRITE) != 0)
    {
      /* Send CMD24, WRITE_BLOCK, and verify good R1 status is returned */

      mmcsd_sendcmdpoll(priv, MMCSD_CMD24, offset);
      ret = mmcsd_recv_r1(priv, MMCSD_CMD24);
      if (ret != OK)
        {
          ferr("ERROR: mmcsd_recv_r1 for CMD24 failed: %d\n", ret);
          SDIO_CANCEL(priv->dev);
          return ret;
        }
    }

  /* Wait for the transfer to complete */

  ret = mmcsd_eventwait(priv, SDIOWAIT_TIMEOUT | SDIOWAIT_ERROR);
  if (ret != OK)
    {
      ferr("ERROR: CMD24 transfer failed: %d\n", ret);
      return ret;
    }

  /* Flag that a write transfer is pending that we will have to check for
   * write complete at the beginning of the next transfer.
   */

  priv->wrbusy = true;

#if defined(CONFIG_MMCSD_SDIOWAIT_WRCOMPLETE)
  /* Arm the write complete detection with timeout */

  SDIO_WAITENABLE(priv->dev, SDIOWAIT_WRCOMPLETE | SDIOWAIT_TIMEOUT,
                  MMCSD_BLOCK_WDATADELAY);
#endif

  /* On success, return the number of blocks written */

  return 1;
}static ssize_t mmcsd_writesingle(FAR struct mmcsd_part_s *part,
                                 FAR const uint8_t *buffer, off_t startblock)
{
  FAR struct mmcsd_state_s *priv = part->priv;
  uint32_t partnum = part - priv->part;
  off_t offset;
  int ret;

  finfo("startblock=%jd\n", (intmax_t)startblock);
  DEBUGASSERT(priv != NULL && buffer != NULL);

  /* Check if the card is locked or write protected (either via software or
   * via the mechanical write protect on the card)
   */

  if (mmcsd_wrprotected(priv))
    {
      ferr("ERROR: Card is locked or write protected\n");
      return -EPERM;
    }

  if (priv->partnum != partnum)
    {
      ret = mmcsd_switch(priv, MMC_CMD6_MODE(MMC_CMD6_MODE_WRITE_BYTE) |
                               MMC_CMD6_INDEX(EXT_CSD_PART_CONF) |
                               MMC_CMD6_VALUE(partnum));
      if (ret != OK)
        {
          ferr("ERROR: mmcsd_switch failed: %d\n", ret);
          return ret;
        }

      priv->partnum = partnum;
    }

#if defined(CONFIG_SDIO_DMA) && defined(CONFIG_ARCH_HAVE_SDIO_PREFLIGHT)
  /* If we think we are going to perform a DMA transfer, make sure that we
   * will be able to before we commit the card to the operation.
   */

  if ((priv->caps & SDIO_CAPS_DMASUPPORTED) != 0)
    {
      ret = SDIO_DMAPREFLIGHT(priv->dev, buffer, priv->blocksize);

      if (ret != OK)
        {
          return ret;
        }
    }
#endif

  /* Verify that the card is ready for the transfer.  The card may still be
   * busy from the preceding write transfer.  It would be simpler to check
   * for write busy at the end of each write, rather than at the beginning of
   * each read AND write, but putting the busy-wait at the beginning of the
   * transfer allows for more overlap and, hopefully, better performance
   */

  ret = mmcsd_transferready(priv);
  if (ret != OK)
    {
      ferr("ERROR: Card not ready: %d\n", ret);
      return ret;
    }

  /* If this is a byte addressed SD card, then convert sector start sector
   * number to a byte offset
   */

  if (IS_BLOCK(priv->type))
    {
      offset = startblock;
    }
  else
    {
      offset = startblock << priv->blockshift;
    }

  finfo("offset=%jd\n", (intmax_t)offset);

  /* Select the block size for the card */

  ret = mmcsd_setblocklen(priv, priv->blocksize);
  if (ret != OK)
    {
      ferr("ERROR: mmcsd_setblocklen failed: %d\n", ret);
      return ret;
    }

  /* If Controller does not need DMA setup before the write then send CMD24
   * now.
   */

  if ((priv->caps & SDIO_CAPS_DMABEFOREWRITE) == 0)
    {
      /* Send CMD24, WRITE_BLOCK, and verify good R1 status is returned */

      ret = mmcsd_sendcmdpoll(priv, MMCSD_CMD24, offset);
      if (ret < 0)
        {
          /* No transfer buffer or data wait has been armed on this path. */
          return ret;
        }

      ret = mmcsd_recv_r1(priv, MMCSD_CMD24);
      if (ret != OK)
        {
          ferr("ERROR: mmcsd_recv_r1 for CMD24 failed: %d\n", ret);
          return ret;
        }
    }

  /* Configure SDIO controller hardware for the write transfer */

  SDIO_BLOCKSETUP(priv->dev, priv->blocksize, 1);
  SDIO_WAITENABLE(priv->dev,
                  SDIOWAIT_TRANSFERDONE | SDIOWAIT_TIMEOUT | SDIOWAIT_ERROR,
                  MMCSD_BLOCK_WDATADELAY);

#ifdef CONFIG_SDIO_DMA
  if ((priv->caps & SDIO_CAPS_DMASUPPORTED) != 0)
    {
      ret = SDIO_DMASENDSETUP(priv->dev, buffer, priv->blocksize);
      if (ret != OK)
        {
          finfo("SDIO_DMASENDSETUP: error %d\n", ret);
          SDIO_CANCEL(priv->dev);
          return ret;
        }
    }
  else
#endif
    {
      SDIO_SENDSETUP(priv->dev, buffer, priv->blocksize);
    }

  /* If Controller needs DMA setup before write then only send CMD24 now. */

  if ((priv->caps & SDIO_CAPS_DMABEFOREWRITE) != 0)
    {
      /* Send CMD24, WRITE_BLOCK, and verify good R1 status is returned */

      mmcsd_sendcmdpoll(priv, MMCSD_CMD24, offset);
      ret = mmcsd_recv_r1(priv, MMCSD_CMD24);
      if (ret != OK)
        {
          ferr("ERROR: mmcsd_recv_r1 for CMD24 failed: %d\n", ret);
          SDIO_CANCEL(priv->dev);
          return ret;
        }
    }

  /* Wait for the transfer to complete */

  ret = mmcsd_eventwait(priv, SDIOWAIT_TIMEOUT | SDIOWAIT_ERROR);
  if (ret != OK)
    {
      ferr("ERROR: CMD24 transfer failed: %d\n", ret);
      return ret;
    }

  /* Flag that a write transfer is pending that we will have to check for
   * write complete at the beginning of the next transfer.
   */

  priv->wrbusy = true;

#if defined(CONFIG_MMCSD_SDIOWAIT_WRCOMPLETE)
  /* Arm the write complete detection with timeout */

  SDIO_WAITENABLE(priv->dev, SDIOWAIT_WRCOMPLETE | SDIOWAIT_TIMEOUT,
                  MMCSD_BLOCK_WDATADELAY);
#endif

  /* On success, return the number of blocks written */

  return 1;
}
static struct mmcsd_state_s dev;
static void reset(int caps,int type,int step,int error){memset(&dev,0,sizeof dev);dev.part[0].priv=&dev;dev.caps=caps;dev.type=type;dev.blocksize=512;dev.blockshift=9;dev.dev=&dev;count=0;failstep=step;errcode=error;}
int main(void){uint8_t data[512]={0};int saved[64],nc;ssize_t rb,rn;bool busy;
for(int caps=0;caps<4;caps++)for(int type=0;type<2;type++)for(int step=0;step<=13;step++)for(int ei=0;ei<3;ei++){
int error=ei==0?-ETIMEDOUT:ei==1?-EIO:-ECANCELED;
reset(caps,type,step,error);rb=baseline_writesingle(&dev.part[0],data,7);nc=count;memcpy(saved,trace,sizeof trace);busy=dev.wrbusy;
reset(caps,type,step,error);rn=mmcsd_writesingle(&dev.part[0],data,7);
if(step==6&&!(caps&2)){assert(rn==error&&!dev.wrbusy);assert(trace[count-1]==6);for(int i=0;i<count;i++)assert(trace[i]!=7&&trace[i]!=8&&trace[i]!=9&&trace[i]!=10&&trace[i]!=11&&trace[i]!=12&&trace[i]!=13);}
else{assert(rn==rb&&count==nc&&dev.wrbusy==busy&&memcmp(trace,saved,count*sizeof(int))==0);}
}
puts("PASS actual writesingle: early CMD24 negative error retained; no later R1/wait/setup/cancel; other paths exact baseline trace/return equivalence. SDIO/card calls stubbed, no device writes.");}
