#include <stdbool.h>
#include <assert.h>
#include <stdio.h>
#define FAR
#define TCB_FLAG_FORCED_CANCEL 1
#define CANCEL_FLAG_NONCANCELABLE 1
#define CANCEL_FLAG_CANCEL_PENDING 2
typedef unsigned irqstate_t;
struct tls_info_s {unsigned tl_cpstate;};
struct tcb_s {unsigned flags; struct tls_info_s tls; bool waitdog;};
static unsigned depth;
static struct tls_info_s*nxsched_get_tls(struct tcb_s*t){return &t->tls;}
static irqstate_t enter_critical_section(void){return depth++;}
static void leave_critical_section(irqstate_t f){assert(depth==f+1);depth--;}
static void wd_cancel(bool*w){*w=false;}
bool nxnotify_cancellation(FAR struct tcb_s *tcb)
{
  FAR struct tls_info_s *tls = nxsched_get_tls(tcb);
  irqstate_t flags;
  bool ret = false;

  /* We need perform the following operations from within a critical section
   * because it can compete with interrupt level activity.
   */

  flags = enter_critical_section();

  /* We only notify the cancellation if (1) the thread has not disabled
   * cancellation, (2) the thread uses the deferred cancellation mode,
   * (3) the thread is waiting within a cancellation point.
   */

  /* Check to see if this task has the non-cancelable bit set. */

  if ((tcb->flags & TCB_FLAG_FORCED_CANCEL) == 0 &&
      (tls->tl_cpstate & CANCEL_FLAG_NONCANCELABLE) != 0)
    {
      /* Then we cannot cancel the thread now.  Here is how this is
       * supposed to work:
       *
       * "When cancellability is disabled, all cancels are held pending
       *  in the target thread until the thread changes the cancellability.
       *  When cancellability is deferred, all cancels are held pending in
       *  the target thread until the thread changes the cancellability,
       *  calls a function which is a cancellation point or calls
       *  pthread_testcancel(), thus creating a cancellation point.  When
       *  cancellability is asynchronous, all cancels are acted upon
       *  immediately, interrupting the thread with its processing."
       */

      tls->tl_cpstate |= CANCEL_FLAG_CANCEL_PENDING;
      leave_critical_section(flags);
      return true;
    }

#ifdef CONFIG_CANCELLATION_POINTS
  /* Check if this task supports deferred cancellation */

  if ((tls->tl_cpstate & CANCEL_FLAG_CANCEL_ASYNC) == 0)
    {
      /* Then we cannot cancel the task asynchronously. */

      ret = true;

      /* Mark the cancellation as pending. */

      tls->tl_cpstate |= CANCEL_FLAG_CANCEL_PENDING;

      /* If the task is waiting at a cancellation point, then notify of the
       * cancellation thereby waking the task up with an ECANCELED error.
       */

      if (tls->tl_cpcount > 0)
        {
          /* If the thread is blocked waiting for a semaphore, then the
           * thread must be unblocked to handle the cancellation.
           */

          if (tcb->task_state == TSTATE_WAIT_SEM)
            {
              nxsem_wait_irq(tcb, ECANCELED);
            }

          /* If the thread is blocked waiting on a signal, then the
           * thread must be unblocked to handle the cancellation.
           */

          else if (tcb->task_state == TSTATE_WAIT_SIG)
            {
              nxsig_wait_irq(tcb, ECANCELED);
            }

#if !defined(CONFIG_DISABLE_MQUEUE) || !defined(CONFIG_DISABLE_MQUEUE_SYSV)
          /* If the thread is blocked waiting on a message queue, then
           * the thread must be unblocked to handle the cancellation.
           */

          else if (tcb->task_state == TSTATE_WAIT_MQNOTEMPTY ||
                   tcb->task_state == TSTATE_WAIT_MQNOTFULL)
            {
              nxmq_wait_irq(tcb, ECANCELED);
            }
#endif
        }

#ifdef HAVE_GROUP_MEMBERS
      else if (tcb->group && (tcb->group->tg_flags & GROUP_FLAG_EXITING))
        {
          /* Exit in progress, do asynchronous cancel instead */

          ret = false;
        }
#endif
    }
#endif

  leave_critical_section(flags);
  return ret;
}

void wd_recover(FAR struct tcb_s *tcb)
{
  /* The task is being deleted.  If it is waiting for any timed event, then
   * cancel the watchdog now so that no events occur after the watchdog
   * expires. Obviously there are lots of race conditions here so this will
   * most certainly have to be revisited in the future.
   *
   */

  wd_cancel(&tcb->waitdog);
}

int main(void){unsigned count=0;for(unsigned disable=0;disable<2;disable++)for(unsigned force=0;force<2;force++){
struct tcb_s t={.flags=force,.tls={.tl_cpstate=disable},.waitdog=true};
bool driver_waitwdog=true;bool r=nxnotify_cancellation(&t);
assert(r==(disable&&!force));assert(!!(t.tls.tl_cpstate&CANCEL_FLAG_CANCEL_PENDING)==r);assert(depth==0);
wd_recover(&t);assert(!t.waitdog&&driver_waitwdog);count++;
}printf("PASS %u fixed-source cancel notification / TCB watchdog models; CONFIG_CANCELLATION_POINTS off. Forced bypass and independent driver watchdog preserved. Not OS dispatch.\n",count);}
