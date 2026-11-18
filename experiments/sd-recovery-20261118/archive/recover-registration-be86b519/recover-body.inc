void nxtask_recover(struct tcb_s *tcb)
{
  recover_notices(tcb); /* PROPOSED NEW integration, not current product */
#if !defined(CONFIG_DISABLE_PTHREAD) && !defined(CONFIG_PTHREAD_MUTEX_UNSAFE)
  /* Recover any mutexes still held by the canceled thread */

  pthread_mutex_inconsistent(tcb);
#endif

  /* The task is being deleted.  Cancel in pending timeout events. */

  wd_recover(tcb);

  /* If the thread holds semaphore counts or is waiting for a semaphore
   *  count, then release the counts.
   */

  nxsem_recover(tcb);

#if !defined(CONFIG_DISABLE_MQUEUE) || !defined(CONFIG_DISABLE_MQUEUE_SYSV)
  /* Handle cases where the thread was waiting for a message queue event */

  nxmq_recover(tcb);
#endif

#ifdef CONFIG_SCHED_SPORADIC
  if ((tcb->flags & TCB_FLAG_POLICY_MASK) == TCB_FLAG_SCHED_SPORADIC)
    {
      /* Stop current sporadic scheduling */

      DEBUGVERIFY(nxsched_stop_sporadic(tcb));
    }
#endif
}
