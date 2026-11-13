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
struct mmcsd_state_s{struct mmcsd_part_s part[1];unsigned partnum;int caps,type,blocksize,blockshift;void*dev;bool wrbusy,wrprotect,locked;};
static int trace[64],count,failstep,errcode;
static int hit(int n){trace[count++]=n;assert(count<64);return failstep==n?errcode:0;}
#define SDIO_WRPROTECTED(d) false
static bool mmcsd_wrprotected(FAR struct mmcsd_state_s *priv)
{
  /* Check if the card is locked (priv->locked) or write protected either (1)
   * via software as reported via the CSD and retained in priv->wrprotect or
   * (2) via the mechanical write protect on the card (which we get from the
   * SDIO driver via SDIO_WRPROTECTED)
   */

#ifdef CONFIG_MMCSD_READONLY
  /* Configuration-enforced read-only applies even when the card and socket
   * report writable media. All SDIO block-write paths use this predicate.
   */

  return true;
#else
  return (priv->wrprotect || priv->locked || SDIO_WRPROTECTED(priv->dev));
#endif
}
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

#define MMCSD_MULTIBLOCK_LIMIT 1
struct inode{void*i_private;};static int locks,unlocks;
static int mmcsd_lock(void*p){(void)p;locks++;return 0;}
static void mmcsd_unlock(void*p){(void)p;unlocks++;}
static ssize_t mmcsd_write(FAR struct inode *inode,
                           FAR const unsigned char *buffer,
                           blkcnt_t startsector, unsigned int nsectors)
{
  FAR struct mmcsd_state_s *priv;
  FAR struct mmcsd_part_s *part;
  size_t sector;
  size_t endsector;
  ssize_t nwrite;
  ssize_t ret = nsectors;

  DEBUGASSERT(inode->i_private);
  part = inode->i_private;
  priv = part->priv;

  finfo("startsector: %" PRIuOFF " nsectors: %u sectorsize: %d\n",
        startsector, nsectors, priv->blocksize);

  if (nsectors > 0)
    {
      ret = mmcsd_lock(priv);
      if (ret < 0)
        {
          return ret;
        }

      ret = nsectors;
      endsector = startsector + nsectors;
      for (sector = startsector; sector < endsector; sector += nwrite)
        {
          /* Write this sector into the user buffer */

#if MMCSD_MULTIBLOCK_LIMIT == 1
          /* Write each block using only the single block transfer method */

          nwrite = mmcsd_writesingle(part, buffer, sector);
#else
          nwrite = endsector - sector;
          if (nwrite > MMCSD_MULTIBLOCK_LIMIT)
            {
              nwrite = MMCSD_MULTIBLOCK_LIMIT;
            }

          if (nwrite == 1)
            {
              nwrite = mmcsd_writesingle(part, buffer, sector);
            }
          else
            {
              nwrite = mmcsd_writemultiple(part, buffer, sector, nwrite);
            }

#endif
          if (nwrite < 0)
            {
              ret = nwrite;
              break;
            }

          /* Increment the buffer pointer by the sector size */

          buffer += nwrite * priv->blocksize;
        }

      mmcsd_unlock(priv);
    }

  /* On success, return the number of blocks written */

  return ret;
}
int main(void){uint8_t data[1536]={0};struct inode node={&dev.part[0]};
for(int caps=0;caps<2;caps++) {locks=unlocks=0;reset(caps,1,6,-ETIMEDOUT);ssize_t r=mmcsd_write(&node,data,7,3);
#ifdef CONFIG_MMCSD_READONLY
assert(r==-EPERM&&count==0&&!dev.wrbusy);
#else
assert(r==-ETIMEDOUT&&!dev.wrbusy);int polls=0;for(int i=0;i<count;i++){polls+=trace[i]==6;assert(trace[i]!=7&&trace[i]!=8&&trace[i]!=9&&trace[i]!=10&&trace[i]!=11&&trace[i]!=12&&trace[i]!=13);}assert(polls==1);
#endif
assert(locks==1&&unlocks==1);}
puts("PASS actual mmcsd_write negative return breaks loop/unlocks; actual CONFIG_MMCSD_READONLY predicate rejects before any command/setup. Device APIs mocked.");}
