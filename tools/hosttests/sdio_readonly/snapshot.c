#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#define FAR
#define OK 0
#define DEBUGASSERT assert
#define finfo(...) ((void)0)
#define ferr(...) ((void)0)
struct mmcsd_state_s;
struct mmcsd_part_s {struct mmcsd_state_s *priv;size_t nblocks;};
struct mmcsd_state_s {bool wrprotect,locked,wrbusy,cmd23support,mediachanged;void*dev;unsigned partnum,caps,blocksize,blockshift,type,rca;struct mmcsd_part_s part[2];};
struct inode {void *i_private;};
struct geometry {bool geo_available,geo_mediachanged,geo_writeenabled;size_t geo_nsectors,geo_sectorsize;};
static int sideeffects;static bool mechanical;
static int forbidden(void){sideeffects++;return -EIO;}
#define SDIO_WRPROTECTED(d) mechanical
#define mmcsd_lock(p) 0
#define mmcsd_unlock(p) ((void)0)
#define IS_EMPTY(p) 0
#define IS_BLOCK(t) 1
#define IS_SD(t) 1
#define IS_MMC(t) 0
#define MMC_CMD6_INDEX(...) forbidden()
#define MMC_CMD6_MODE(...) forbidden()
#define MMC_CMD6_VALUE(...) forbidden()
#define SDIO_BLOCKSETUP(...) forbidden()
#define SDIO_CANCEL(...) forbidden()
#define SDIO_DMAPREFLIGHT(...) forbidden()
#define SDIO_DMASENDSETUP(...) forbidden()
#define SDIO_SENDSETUP(...) forbidden()
#define SDIO_WAITENABLE(...) forbidden()
#define mmcsd_eventwait(...) forbidden()
#define mmcsd_recv_r1(...) forbidden()
#define mmcsd_sendcmdpoll(...) forbidden()
#define mmcsd_setblockcount(...) forbidden()
#define mmcsd_setblocklen(...) forbidden()
#define mmcsd_stoptransmission(...) forbidden()
#define mmcsd_switch(...) forbidden()
#define mmcsd_transferready(...) forbidden()
#define EXT_CSD_PART_CONF 1
#define MMCSD_BLOCK_WDATADELAY 1
#define MMCSD_CMD24 1
#define MMCSD_CMD25 1
#define MMCSD_CMD56WR 1
#define MMCSD_PART_RPMB 1
#define SDIOWAIT_ERROR 1
#define SDIOWAIT_TIMEOUT 1
#define SDIOWAIT_TRANSFERDONE 1
#define SDIOWAIT_WRCOMPLETE 1
#define SDIO_CAPS_DMABEFOREWRITE 1
#define SDIO_CAPS_DMASUPPORTED 1
#define SD_ACMD23 1
#define SD_CMD55 1
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
static ssize_t mmcsd_writesingle(FAR struct mmcsd_part_s *part,
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
}
static ssize_t mmcsd_writemultiple(FAR struct mmcsd_part_s *part,
                                   FAR const uint8_t *buffer,
                                   off_t startblock, size_t nblocks)
{
  FAR struct mmcsd_state_s *priv = part->priv;
  size_t nbytes = nblocks << priv->blockshift;
  uint32_t partnum = part - priv->part;
  off_t  offset;
  int ret;
  int evret = OK;

  finfo("startblock=%jd nblocks=%zu\n", (intmax_t)startblock, nblocks);
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
      ret = SDIO_DMAPREFLIGHT(priv->dev, buffer, nbytes);

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

  /* If this is a byte addressed SD card, then convert both the total
   * transfer size to bytes and the sector start sector number to a byte
   * offset
   */

  if (IS_BLOCK(priv->type))
    {
      offset = startblock;
    }
  else
    {
      offset = startblock << priv->blockshift;
    }

  finfo("nbytes=%zu byte offset=%jd\n", nbytes, (intmax_t)offset);

  /* Select the block size for the card */

  ret = mmcsd_setblocklen(priv, priv->blocksize);
  if (ret != OK)
    {
      ferr("ERROR: mmcsd_setblocklen failed: %d\n", ret);
      return ret;
    }

  /* If this is an SD card, then send ACMD23 (SET_WR_BLK_ERASE_COUNT) just
   * before sending CMD25 (WRITE_MULTIPLE_BLOCK).  This sets the number of
   * write blocks to be pre-erased and might make the following multiple
   * block write command faster.
   */

  if (IS_SD(priv->type))
    {
      /* Send CMD55, APP_CMD, a verify that good R1 status is returned */

      mmcsd_sendcmdpoll(priv, SD_CMD55, (uint32_t)priv->rca << 16);
      ret = mmcsd_recv_r1(priv, SD_CMD55);
      if (ret != OK)
        {
          ferr("ERROR: mmcsd_recv_r1 for CMD55 (ACMD23) failed: %d\n", ret);
          return ret;
        }

      /* Send CMD23, SET_WR_BLK_ERASE_COUNT, and verify that good R1 status
       * is returned.
       */

      mmcsd_sendcmdpoll(priv, SD_ACMD23, nblocks);
      ret = mmcsd_recv_r1(priv, SD_ACMD23);
      if (ret != OK)
        {
          ferr("ERROR: mmcsd_recv_r1 for ACMD23 failed: %d\n", ret);
          return ret;
        }
    }

  /* Data to the RPMB is programmed with the WRITE_MULTIPLE_BLOCK(CMD25),
   * in prior to the command CMD25 the block count is set by CMD23,
   * with argument bit [31] set as 1 to indicate Reliable Write type of
   * programming access.
   */

#ifdef CONFIG_MMCSD_MMCSUPPORT
  if (IS_MMC(priv->type))
    {
      ret = mmcsd_setblockcount(priv, priv->partnum == MMCSD_PART_RPMB ?
                                ((1 << 31) | nblocks) : nblocks);
      if (ret != OK)
        {
          return ret;
        }
    }
  else
#endif
  if (IS_SD(priv->type) && priv->cmd23support)
    {
      ret = mmcsd_setblockcount(priv, nblocks);
      if (ret != OK)
        {
          return ret;
        }
    }

  /* If Controller does not need DMA setup before the write then send CMD25
   * now.
   */

  if ((priv->caps & SDIO_CAPS_DMABEFOREWRITE) == 0)
    {
      /* Send CMD25, WRITE_MULTIPLE_BLOCK, and verify that good R1 status
       * is returned
       */

      mmcsd_sendcmdpoll(priv, MMCSD_CMD25, offset);
      ret = mmcsd_recv_r1(priv, MMCSD_CMD25);
      if (ret != OK)
        {
          ferr("ERROR: mmcsd_recv_r1 for CMD25 failed: %d\n", ret);
          return ret;
        }
    }

  /* Configure SDIO controller hardware for the write transfer */

  SDIO_BLOCKSETUP(priv->dev, priv->blocksize, nblocks);
  SDIO_WAITENABLE(priv->dev,
                  SDIOWAIT_TRANSFERDONE | SDIOWAIT_TIMEOUT | SDIOWAIT_ERROR,
                  nblocks * MMCSD_BLOCK_WDATADELAY);

#ifdef CONFIG_SDIO_DMA
  if ((priv->caps & SDIO_CAPS_DMASUPPORTED) != 0)
    {
      ret = SDIO_DMASENDSETUP(priv->dev, buffer, nbytes);
      if (ret != OK)
        {
          ferr("SDIO_DMASENDSETUP: error %d\n", ret);
          SDIO_CANCEL(priv->dev);
          return ret;
        }
    }
  else
#endif
    {
      SDIO_SENDSETUP(priv->dev, buffer, nbytes);
    }

  /* If Controller needs DMA setup before write then only send CMD25 now. */

  if ((priv->caps & SDIO_CAPS_DMABEFOREWRITE) != 0)
    {
      /* Send CMD25, WRITE_MULTIPLE_BLOCK, and verify that good R1 status
       * is returned
       */

      mmcsd_sendcmdpoll(priv, MMCSD_CMD25, offset);
      ret = mmcsd_recv_r1(priv, MMCSD_CMD25);
      if (ret != OK)
        {
          ferr("ERROR: mmcsd_recv_r1 for CMD25 failed: %d\n", ret);
          SDIO_CANCEL(priv->dev);
          return ret;
        }
    }

  /* Wait for the transfer to complete */

  evret = mmcsd_eventwait(priv, SDIOWAIT_TIMEOUT | SDIOWAIT_ERROR);
  if (evret != OK)
    {
      ferr("ERROR: CMD25 transfer failed: %d\n", evret);

      /* If we return from here, we probably leave the sd-card in
       * Receive-data State. Instead, we will remember that
       * an error occurred and try to execute the STOP_TRANSMISSION
       * to put the sd-card back into Transfer State.
       */
    }

  if (IS_SD(priv->type) && !priv->cmd23support)
    {
      /* Send STOP_TRANSMISSION */

      ret = mmcsd_stoptransmission(priv);
      if (evret != OK)
        {
          return evret;
        }

      if (ret != OK)
        {
          ferr("ERROR: mmcsd_stoptransmission failed: %d\n", ret);
          return ret;
        }
    }
  else
    {
      if (evret != OK)
        {
          return evret;
        }
    }

  /* Flag that a write transfer is pending that we will have to check for
   * write complete at the beginning of the next transfer.
   */

  priv->wrbusy = true;

#if defined(CONFIG_MMCSD_SDIOWAIT_WRCOMPLETE)
  /* Arm the write complete detection with timeout */

  SDIO_WAITENABLE(priv->dev, SDIOWAIT_WRCOMPLETE | SDIOWAIT_TIMEOUT,
                  nblocks * MMCSD_BLOCK_WDATADELAY);
#endif

  /* On success, return the number of blocks written */

  return nblocks;
}
static int mmcsd_general_cmd_write(FAR struct mmcsd_state_s *priv,
                                   FAR const uint8_t *buffer,
                                   off_t startblock)
{
  int ret;

  DEBUGASSERT(priv != NULL && buffer != NULL);

  /* Check if the card is locked or write protected (either via software or
   * via the mechanical write protect on the card)
   */

  if (mmcsd_wrprotected(priv))
    {
      ferr("ERROR: Card is locked or write protected\n");
      return -EPERM;
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

  /* Select the block size for the card */

  ret = mmcsd_setblocklen(priv, priv->blocksize);
  if (ret != OK)
    {
      ferr("ERROR: mmcsd_setblocklen failed: %d\n", ret);
      return ret;
    }

  /* If Controller does not need DMA setup before the write then send CMD56
   * now.
   */

  if ((priv->caps & SDIO_CAPS_DMABEFOREWRITE) == 0)
    {
      /* Send CMD56, WRITE_BLOCK, and verify good R1 status is returned */

      mmcsd_sendcmdpoll(priv, MMCSD_CMD56WR, startblock);
      ret = mmcsd_recv_r1(priv, MMCSD_CMD56WR);
      if (ret != OK)
        {
          ferr("ERROR: mmcsd_recv_r1 for CMD56 failed: %d\n", ret);
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
      /* Send CMD56, WRITE_BLOCK, and verify good R1 status is returned */

      mmcsd_sendcmdpoll(priv, MMCSD_CMD56WR, startblock);
      ret = mmcsd_recv_r1(priv, MMCSD_CMD56WR);
      if (ret != OK)
        {
          ferr("ERROR: mmcsd_recv_r1 for CMD56 failed: %d\n", ret);
          SDIO_CANCEL(priv->dev);
          return ret;
        }
    }

  /* Wait for the transfer to complete */

  ret = mmcsd_eventwait(priv, SDIOWAIT_TIMEOUT | SDIOWAIT_ERROR);
  if (ret != OK)
    {
      ferr("ERROR: CMD56 transfer failed: %d\n", ret);
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

  /* On success, return OK */

  return OK;
}
static int mmcsd_geometry(FAR struct inode *inode, struct geometry *geometry)
{
  FAR struct mmcsd_state_s *priv;
  FAR struct mmcsd_part_s *part;
  int ret = -EINVAL;

  finfo("Entry\n");
  DEBUGASSERT(inode->i_private);

  if (geometry)
    {
      memset(geometry, 0, sizeof(*geometry));

      /* Is there a (supported) card inserted in the slot? */

      part = inode->i_private;
      priv = part->priv;

      ret = mmcsd_lock(priv);
      if (ret < 0)
        {
          return ret;
        }

      if (IS_EMPTY(priv))
        {
          /* No.. return ENODEV */

          finfo("IS_EMPTY\n");
          ret = -ENODEV;
        }
      else
        {
          /* Yes.. return the geometry of the card */

          geometry->geo_available     = true;
          geometry->geo_mediachanged  = priv->mediachanged;
          geometry->geo_writeenabled  = !mmcsd_wrprotected(priv);
          geometry->geo_nsectors      = part->nblocks;
          geometry->geo_sectorsize    = priv->blocksize;

          finfo("available: true mediachanged: %s writeenabled: %s\n",
                 geometry->geo_mediachanged ? "true" : "false",
                 geometry->geo_writeenabled ? "true" : "false");
          finfo("nsectors: %" PRIuOFF " sectorsize: %" PRIi16 "\n",
                 geometry->geo_nsectors,
                 geometry->geo_sectorsize);

          priv->mediachanged = false;
          ret = OK;
        }

      mmcsd_unlock(priv);
    }

  return ret;
}
int main(void){struct mmcsd_state_s p={.blocksize=512,.blockshift=9};p.part[0].priv=&p;p.part[0].nblocks=100;struct inode inode={&p.part[0]};uint8_t buffer[1024]={0};
for(unsigned bits=0;bits<8;bits++){p.wrprotect=bits&1;p.locked=bits&2;mechanical=bits&4;bool expected=bits!=0;
#ifdef CONFIG_MMCSD_READONLY
expected=true;
#endif
assert(mmcsd_wrprotected(&p)==expected);struct geometry g;assert(mmcsd_geometry(&inode,&g)==0);assert(g.geo_writeenabled==!expected);
sideeffects=0;ssize_t a=mmcsd_writesingle(&p.part[0],buffer,0);ssize_t b=mmcsd_writemultiple(&p.part[0],buffer,0,2);ssize_t c=mmcsd_general_cmd_write(&p,buffer,0);
if(expected){assert(a==-EPERM&&b==-EPERM&&c==-EPERM);assert(sideeffects==0);}else{assert(a==-EIO&&b==-EIO&&c==-EIO);assert(sideeffects==3);}}
puts("PASS actual full write functions: protected reject before any lower operation; geometry; option-off protection truth table");}
