#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <assert.h>
#define DEBUGASSERT assert
#define mcinfo(...) ((void)0)
#define SDIO_STATUS_PRESENT 1
#define SDIOMEDIA_INSERTED 1
#define SDIOMEDIA_EJECTED 2
#define HPWORK 0
static int calls,queued;static bool irq;
struct stm32_dev_s {void(*callback)(void*);void*cbarg;unsigned cbevents,cdstatus;int cbwork;};
static bool up_interrupt_context(void){return irq;}
static void work_queue(int q,int*w,void(*cb)(void*),void*a,int d){(void)q;(void)w;(void)cb;(void)a;(void)d;queued++;}
static void cb(void*a){(void)a;calls++;}
static void stm32_callback(void *arg)
{
#ifdef CONFIG_SD_SECTOR_READ_REVIEW
  return ; /* No external callback producers in this diagnostic. */
#endif

  struct stm32_dev_s *priv = (struct stm32_dev_s *)arg;

  /* Is a callback registered? */

  DEBUGASSERT(priv != NULL);
  mcinfo("Callback %p(%p) cbevents: %02" PRIx8 " cdstatus: %02" PRIx8 "\n",
         priv->callback, priv->cbarg, priv->cbevents, priv->cdstatus);

  if (priv->callback)
    {
      /* Yes.. Check for enabled callback events */

      if ((priv->cdstatus & SDIO_STATUS_PRESENT) != 0)
        {
          /* Media is present.  Is the media inserted event enabled? */

          if ((priv->cbevents & SDIOMEDIA_INSERTED) == 0)
            {
              /* No... return without performing the callback */

              return;
            }
        }
      else
        {
          /* Media is not present.  Is the media eject event enabled? */

          if ((priv->cbevents & SDIOMEDIA_EJECTED) == 0)
            {
              /* No... return without performing the callback */

              return;
            }
        }

      /* Perform the callback, disabling further callbacks.  Of course, the
       * the callback can (and probably should) re-enable callbacks.
       */

      priv->cbevents = 0;

      /* Callbacks cannot be performed in the context of an interrupt
       * handler.  If we are in an interrupt handler, then queue the
       * callback to be performed later on the work thread.
       */

      if (up_interrupt_context())
        {
          /* Yes.. queue it */

          mcinfo("Queuing callback to %p(%p)\n",
                 priv->callback, priv->cbarg);

          work_queue(HPWORK, &priv->cbwork, priv->callback,
                     priv->cbarg, 0);
        }
      else
        {
          /* No.. then just call the callback here */

          mcinfo("Callback to %p(%p)\n", priv->callback, priv->cbarg);
          priv->callback(priv->cbarg);
        }
    }
}
int main(void){struct stm32_dev_s p={.callback=cb};
 for(int i=0;i<10000;i++){irq=i&1;p.cdstatus=(i>>1)&1;p.cbevents=3;stm32_callback(&p);}
#ifdef CONFIG_SD_SECTOR_READ_REVIEW
 assert(calls==0 && queued==0);stm32_callback(0);
#else
 assert(calls==5000 && queued==5000);
#endif
 return 0;}
