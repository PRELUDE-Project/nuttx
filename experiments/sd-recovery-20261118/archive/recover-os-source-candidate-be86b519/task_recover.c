/****************************************************************************
 * sched/task/task_recover.c
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.  The
 * ASF licenses this file to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance with the
 * License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
 * License for the specific language governing permissions and limitations
 * under the License.
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <assert.h>

#include <nuttx/arch.h>
#include <nuttx/wdog.h>
#include <nuttx/sched.h>

#include "semaphore/semaphore.h"
#include "wdog/wdog.h"
#include "mqueue/mqueue.h"
#include "pthread/pthread.h"
#include "sched/sched.h"
#include "task/task.h"

#ifdef CONFIG_SD_SECTOR_READ_REVIEW
#  include <errno.h>
#  include <stdbool.h>
#  include <nuttx/irq.h>
#  include <nuttx/recovery_watch.h>
static struct nxsched_recovery_watch_s *head;
static bool dispatching;
#define critical_depth depth
int nxsched_recovery_bind_locked(struct nxsched_recovery_watch_s *n, struct tcb_s *owner, uint64_t gen,
                       void (*notify)(struct nxsched_recovery_watch_s *, uint64_t))
{
  struct nxsched_recovery_watch_s *p;
  /* Caller holds the lifetime-protecting critical section. */
  if (dispatching) return -EBUSY;
  if (!n || !owner || !notify || !gen) return -EINVAL;
  for (p = head; p; p = p->next) if (p == n) return -EBUSY;
  n->owner = owner; n->generation = gen; n->notify = notify;
  n->next = head; head = n;
  return 0;
}
int nxsched_recovery_unbind_locked(struct nxsched_recovery_watch_s *n, struct tcb_s *owner, uint64_t gen)
{
  struct nxsched_recovery_watch_s **p = &head;
  /* Caller holds the lifetime-protecting critical section. */
  while (*p && *p != n) p = &(*p)->next;
  if (!*p || n->owner != owner || n->generation != gen) return -ESTALE;
  *p = n->next;
  n->next = NULL; n->owner = NULL;
  return 0;
}
static void nxsched_recovery_notify_locked(struct tcb_s *owner)
{
  struct nxsched_recovery_watch_s **p = &head;
  DEBUGASSERT(!dispatching);
  dispatching = true;
  while (*p) {
    struct nxsched_recovery_watch_s *n = *p;
    if (n->owner != owner) { p = &n->next; continue; }
    /* Unlink before notifying; lifetime remains static, no queued callback. */
    *p = n->next; n->next = NULL; n->owner = NULL;
    n->notify(n, n->generation);
    p = &head;
  }
  dispatching = false;
}
#endif

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: nxtask_recover
 *
 * Description:
 *   This function is called when a task is deleted via task_delete() or
 *   via pthread_cancel.  I checks checks for semaphores, message queue, and
 *   watchdog timer resources stranded in bad conditions.
 *
 * Input Parameters:
 *   tcb - The TCB of the terminated task or thread
 *
 * Returned Value:
 *   None.
 *
 * Assumptions:
 *   This function is called from task deletion logic in a safe context.
 *
 ****************************************************************************/

void nxtask_recover(FAR struct tcb_s *tcb)
{
#ifdef CONFIG_SD_SECTOR_READ_REVIEW
  irqstate_t recovery_flags = enter_critical_section();
  nxsched_recovery_notify_locked(tcb);
  leave_critical_section(recovery_flags);
#endif

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
