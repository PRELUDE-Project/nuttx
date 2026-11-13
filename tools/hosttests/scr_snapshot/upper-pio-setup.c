
#include <stddef.h>
#include <stdint.h>
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#define OK 0
#define SDIO_CAPS_DMASUPPORTED 1
#define SDIOWAIT_TRANSFERDONE 1
#define SDIOWAIT_TIMEOUT 2
#define SDIOWAIT_ERROR 4
#define MMCSD_BLOCK_RDATADELAY 10
#define ferr(...) ((void)0)
#define finfo(...) ((void)0)
struct mmcsd_state_s{void*dev;unsigned caps,blocksize,type,cmd23support;};
static int setup_result,cancel_result,commands,cancels,setups,dmas,waits,blocks;
#define IS_SD(t) ((t)==1)
static int mmcsd_setblockcount(struct mmcsd_state_s*p,size_t n){(void)p;(void)n;return 0;}
static int setup(void*d,void*b,size_t n){(void)d;(void)b;assert(n>0);setups++;return setup_result;}
static int dma(void*d,void*b,size_t n){dmas++;return setup(d,b,n);}
static int cancel(void*d){(void)d;cancels++;return cancel_result;}
#define SDIO_RECVSETUP setup
#define SDIO_DMARECVSETUP dma
#define SDIO_CANCEL cancel
#define SDIO_BLOCKSETUP(d,b,n) ((void)(d),(void)(b),(void)(n),blocks++)
#define SDIO_WAITENABLE(d,e,t) ((void)(d),(void)(e),(void)(t),waits++)
static int test_mmcsd_readsingle(struct mmcsd_state_s*priv,uint8_t*buffer,size_t nblocks){
int ret;size_t nbytes=nblocks*priv->blocksize;uint8_t*extcsd=buffer;(void)nbytes;(void)extcsd;
  SDIO_BLOCKSETUP(priv->dev, priv->blocksize, 1);
  SDIO_WAITENABLE(priv->dev,
                  SDIOWAIT_TRANSFERDONE | SDIOWAIT_TIMEOUT | SDIOWAIT_ERROR,
                  MMCSD_BLOCK_RDATADELAY);

#ifdef CONFIG_SDIO_DMA
  if ((priv->caps & SDIO_CAPS_DMASUPPORTED) != 0)
    {
      ret = SDIO_DMARECVSETUP(priv->dev, buffer, priv->blocksize);
      if (ret != OK)
        {
          finfo("SDIO_DMARECVSETUP: error %d\n", ret);
          SDIO_CANCEL(priv->dev);
          return ret;
        }
    }
  else
#endif
    {
      ret = SDIO_RECVSETUP(priv->dev, buffer, priv->blocksize);
      if (ret != OK)
        {
          ferr("ERROR: SDIO_RECVSETUP failed: %d\n", ret);
          SDIO_CANCEL(priv->dev);
          return ret;
        }
    }

commands++;return 0;
}
static int test_mmcsd_readmultiple(struct mmcsd_state_s*priv,uint8_t*buffer,size_t nblocks){
int ret;size_t nbytes=nblocks*priv->blocksize;uint8_t*extcsd=buffer;(void)nbytes;(void)extcsd;
  SDIO_BLOCKSETUP(priv->dev, priv->blocksize, nblocks);
  SDIO_WAITENABLE(priv->dev,
                  SDIOWAIT_TRANSFERDONE | SDIOWAIT_TIMEOUT | SDIOWAIT_ERROR,
                  nblocks * MMCSD_BLOCK_RDATADELAY);

#ifdef CONFIG_SDIO_DMA
  if ((priv->caps & SDIO_CAPS_DMASUPPORTED) != 0)
    {
      ret = SDIO_DMARECVSETUP(priv->dev, buffer, nbytes);
      if (ret != OK)
        {
          finfo("SDIO_DMARECVSETUP: error %d\n", ret);
          SDIO_CANCEL(priv->dev);
          return ret;
        }
    }
  else
#endif
    {
      ret = SDIO_RECVSETUP(priv->dev, buffer, nbytes);
      if (ret != OK)
        {
          ferr("ERROR: SDIO_RECVSETUP failed: %d\n", ret);
          SDIO_CANCEL(priv->dev);
          return ret;
        }
    }

#ifdef CONFIG_MMCSD_MMCSUPPORT
  if (IS_MMC(priv->type) || (IS_SD(priv->type) && priv->cmd23support))
#else
  if (IS_SD(priv->type) && priv->cmd23support)
#endif
    {
      ret = mmcsd_setblockcount(priv, nblocks);
      if (ret != OK)
        {
          return ret;
        }
    }

commands++;return 0;
}
static int test_mmcsd_read_extcsd(struct mmcsd_state_s*priv,uint8_t*buffer,size_t nblocks){
int ret;size_t nbytes=nblocks*priv->blocksize;uint8_t*extcsd=buffer;(void)nbytes;(void)extcsd;
  SDIO_BLOCKSETUP(priv->dev, 512, 1);
  SDIO_WAITENABLE(priv->dev,
                  SDIOWAIT_TRANSFERDONE | SDIOWAIT_TIMEOUT | SDIOWAIT_ERROR,
                  MMCSD_BLOCK_RDATADELAY);

#ifdef CONFIG_SDIO_DMA
  if ((priv->caps & SDIO_CAPS_DMASUPPORTED) != 0)
    {
      finfo("Setting up for DMA transfer.\n");
      ret = SDIO_DMARECVSETUP(priv->dev, extcsd, 512);
      if (ret != OK)
        {
          ferr("SDIO_DMARECVSETUP: error %d\n", ret);
          SDIO_CANCEL(priv->dev);
          return ret;
        }
    }
  else
#endif
    {
      ret = SDIO_RECVSETUP(priv->dev, extcsd, 512);
      if (ret != OK)
        {
          ferr("ERROR: SDIO_RECVSETUP failed: %d\n", ret);
          SDIO_CANCEL(priv->dev);
          return ret;
        }
    }

commands++;return 0;
}
static int test_mmcsd_general_cmd_read(struct mmcsd_state_s*priv,uint8_t*buffer,size_t nblocks){
int ret;size_t nbytes=nblocks*priv->blocksize;uint8_t*extcsd=buffer;(void)nbytes;(void)extcsd;
  SDIO_BLOCKSETUP(priv->dev, priv->blocksize, 1);
  SDIO_WAITENABLE(priv->dev,
                  SDIOWAIT_TRANSFERDONE | SDIOWAIT_TIMEOUT | SDIOWAIT_ERROR,
                  MMCSD_BLOCK_RDATADELAY);

#ifdef CONFIG_SDIO_DMA
  if ((priv->caps & SDIO_CAPS_DMASUPPORTED) != 0)
    {
      ret = SDIO_DMARECVSETUP(priv->dev, buffer, priv->blocksize);
      if (ret != OK)
        {
          finfo("SDIO_DMARECVSETUP: error %d\n", ret);
          SDIO_CANCEL(priv->dev);
          return ret;
        }
    }
  else
#endif
    {
      ret = SDIO_RECVSETUP(priv->dev, buffer, priv->blocksize);
      if (ret != OK)
        {
          ferr("ERROR: SDIO_RECVSETUP failed: %d\n", ret);
          SDIO_CANCEL(priv->dev);
          return ret;
        }
    }

commands++;return 0;
}

int main(void){uint8_t b[1024];struct mmcsd_state_s d={.blocksize=512};int(*fn[])(struct mmcsd_state_s*,uint8_t*,size_t)={test_mmcsd_readsingle,test_mmcsd_readmultiple,test_mmcsd_read_extcsd,test_mmcsd_general_cmd_read};int errors[]={0,-ENOTSUP,-EBUSY,-EIO};
for(unsigned f=0;f<4;f++)for(unsigned cap=0;cap<2;cap++)for(unsigned e=0;e<4;e++)for(unsigned ce=0;ce<2;ce++){
d.caps=cap;setup_result=errors[e];cancel_result=ce?-EINPROGRESS:0;commands=cancels=setups=dmas=waits=blocks=0;
int r=fn[f](&d,b,2);assert(r==setup_result&&setups==1&&waits==1&&blocks==1);
assert(commands==(e==0)&&cancels==(e!=0));
#ifdef CONFIG_SDIO_DMA
assert(dmas==(int)cap);
#else
assert(dmas==0);
#endif
}
puts("PASS four extracted upper setup-to-command paths: original setup errno preserved, one cancel on failure, no following data command, normal continuation and DMA/PIO capability branches. Preconditions/MMIO mocked; cancel not physical stop.");}
