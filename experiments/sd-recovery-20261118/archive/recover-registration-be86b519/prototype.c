/* DESIGN PROTOTYPE ONLY: this API does not exist in NuttX.
 * Single CPU, ordinary IRQ exclusion, static records, synchronous callbacks.
 * Scheduler integration and actual driver integration are NOT implemented.
 */
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <errno.h>
#include <stdio.h>
struct tcb_s { unsigned id; };
struct notice {
  struct notice *next;
  struct tcb_s *owner;
  uint64_t generation;
  void (*notify)(struct notice *, uint64_t);
};
static struct notice *head;
static bool dispatching;
static unsigned critical_depth;
static int bind_notice(struct notice *n, struct tcb_s *owner, uint64_t gen,
                       void (*notify)(struct notice *, uint64_t))
{
  struct notice *p;
  assert(critical_depth);
  if (dispatching) return -EBUSY;
  if (!n || !owner || !notify || !gen) return -EINVAL;
  for (p = head; p; p = p->next) if (p == n) return -EBUSY;
  n->owner = owner; n->generation = gen; n->notify = notify;
  n->next = head; head = n;
  return 0;
}
static int unbind_notice(struct notice *n, struct tcb_s *owner, uint64_t gen)
{
  struct notice **p = &head;
  assert(critical_depth);
  while (*p && *p != n) p = &(*p)->next;
  if (!*p || n->owner != owner || n->generation != gen) return -ESTALE;
  *p = n->next;
  n->next = NULL; n->owner = NULL;
  return 0;
}
static void recover_notices(struct tcb_s *owner)
{
  struct notice **p = &head;
  assert(critical_depth && !dispatching);
  dispatching = true;
  while (*p) {
    struct notice *n = *p;
    if (n->owner != owner) { p = &n->next; continue; }
    /* Unlink before notifying; lifetime remains static, no queued callback. */
    *p = n->next; n->next = NULL; n->owner = NULL;
    n->notify(n, n->generation);
    p = &head;
  }
  dispatching = false;
}
static unsigned order, calls, stage, expected_stage;
static struct tcb_s a = {1}, b = {2};
static struct notice node;
static uint64_t lease_generation;
static void notify(struct notice *n, uint64_t gen)
{
  assert(!n->owner && critical_depth);
  assert(order == 0);
  assert(gen == lease_generation);
  assert(bind_notice(n, &b, gen + 1, notify) == -EBUSY);
  ++calls; stage = 1;
}
/* These doubles test order only; they do not execute NuttX cleanup. */
static void pthread_mutex_inconsistent(struct tcb_s *t) {(void)t; assert(stage == expected_stage); ++order;}
static void wd_recover(struct tcb_s *t) {(void)t; assert(order == 1); ++order;}
static void nxsem_recover(struct tcb_s *t) {(void)t; assert(order == 2); ++order;}
static void nxmq_recover(struct tcb_s *t) {(void)t; assert(order == 3); ++order;}
#include "recover-body.inc"
int main(void)
{
  critical_depth = 1;
  lease_generation = 1; expected_stage = 1;
  assert(bind_notice(NULL, &a, 1, notify) == -EINVAL);
  assert(bind_notice(&node, &a, 1, notify) == 0);
  assert(bind_notice(&node, &a, 1, notify) == -EBUSY);
  nxtask_recover(&a); assert(calls == 1 && !head && order == 4);
  order = 0; stage = expected_stage = 0; nxtask_recover(&a); assert(calls == 1);
  /* Same TCB after restart, but old registration was consumed. */
  order = 0; stage = 0; expected_stage = 1; lease_generation = 2;
  assert(bind_notice(&node, &a, 2, notify) == 0);
  nxtask_recover(&a); assert(calls == 2);
  /* Successful SCR HANDOFF must disarm old-owner notice. */
  order = 0; assert(bind_notice(&node, &a, 3, notify) == 0);
  assert(unbind_notice(&node, &a, 3) == 0); lease_generation = 4;
  assert(bind_notice(&node, &b, 4, notify) == 0);
  assert(unbind_notice(&node, &a, 3) == -ESTALE);
  assert(head == &node && node.owner == &b && node.generation == 4);
  stage = expected_stage = 0;
  nxtask_recover(&a); assert(calls == 2 && head == &node);
  order = 0; expected_stage = 1; nxtask_recover(&b); assert(calls == 3 && !head);
  puts("PASS: recover ordering, duplicate/reentrant binding, restart reuse, HANDOFF");
}
