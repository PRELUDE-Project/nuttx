#ifndef SCR_LEASE_REVIEW_H
#define SCR_LEASE_REVIEW_H
/* REVIEW ONLY. Every call and observation requires the SAME caller-provided
 * lock, including ISR, timeout, setup, copy, reset and initialization paths.
 * This header neither acquires that lock nor proves its suitability for SMP.
 * Storage must persist for controller lifetime, be DMA-accessible, and have
 * exclusive cache lines. Alignment alone does not establish those properties.
 * No hardware stopping, cache maintenance, notification or reset is performed.
 * Never reinitialize a used instance: retirement and quarantine are terminal.
 */
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <errno.h>
#include <stdalign.h>

enum scr_lease_state { SCR_LEASE_IDLE, SCR_LEASE_ACTIVE,
                       SCR_LEASE_QUARANTINED, SCR_LEASE_RETIRED };
struct scr_lease_review;
struct scr_lease_key { const struct scr_lease_review *instance;
                       uintptr_t owner; uint64_t generation; };
struct scr_lease_review {
  alignas(32) uint8_t arena[32];
  enum scr_lease_state state;
  uintptr_t owner;
  uint64_t generation;
  void *cpu_destination;
  int result;
};
/* Static zero initialization only, before any possible hardware access. */
static inline bool scr_lease_matches(const struct scr_lease_review *s,
                                     struct scr_lease_key k)
{
  return k.instance == s && k.owner != 0 && s->owner == k.owner &&
         k.generation != 0 && s->generation == k.generation;
}
static inline int scr_lease_begin(struct scr_lease_review *s, uintptr_t owner,
                                  void *destination, struct scr_lease_key *key)
{
  if (!owner || !destination || !key) return -EINVAL;
  if (s->state != SCR_LEASE_IDLE) return -EBUSY;
  /* Never wrap a generation or silently reset one. */
  if (s->generation == UINT64_MAX) return -EOVERFLOW;
  s->generation++;
  s->owner = owner;
  s->cpu_destination = destination;
  s->result = -EINPROGRESS;
  s->state = SCR_LEASE_ACTIVE;
  key->instance = s;
  key->owner = owner;
  key->generation = s->generation;
  return 0;
}
static inline bool scr_lease_active(const struct scr_lease_review *s,
                                    struct scr_lease_key k)
{
  return s->state == SCR_LEASE_ACTIVE && scr_lease_matches(s, k);
}
/* A stale event is rejected, not interpreted as belonging to the new owner.
 * Caller must invoke this BEFORE wakeup/return, under the shared lock.
 * First terminal error is retained. A later real fault may tighten RETIRED
 * to QUARANTINED; it can never reopen admission.
 */
static inline int scr_lease_fault(struct scr_lease_review *s,
                                  struct scr_lease_key k, int error)
{
  if (error >= 0) return -EINVAL;
  if (!scr_lease_matches(s, k) || s->state == SCR_LEASE_IDLE) return -ESTALE;
  s->cpu_destination = NULL;
  if (s->state != SCR_LEASE_QUARANTINED) s->result = error;
  s->state = SCR_LEASE_QUARANTINED;
  return s->result;
}
/* Apparent success is NOT authority to copy, invalidate, or reuse arena.
 * All CPU destinations are revoked even for successful completion.
 */
static inline int scr_lease_retire(struct scr_lease_review *s,
                                   struct scr_lease_key k)
{
  if (!scr_lease_matches(s, k) || s->state == SCR_LEASE_IDLE) return -ESTALE;
  s->cpu_destination = NULL;
  if (s->state == SCR_LEASE_QUARANTINED) return s->result;
  s->state = SCR_LEASE_RETIRED;
  s->result = -EAGAIN;
  return s->result;
}
/* End seals an active lease too: never return to caller leaving an ACTIVE
 * CPU destination. Preserve a negative operation errno, otherwise fail closed.
 */
static inline int scr_lease_end(struct scr_lease_review *s,
                                struct scr_lease_key k, int operation_result)
{
  if (!scr_lease_matches(s, k) || s->state == SCR_LEASE_IDLE) return -ESTALE;
  if (operation_result < 0) return scr_lease_fault(s, k, operation_result);
  return scr_lease_retire(s, k);
}
struct sdio_dev_s;
int stm32_scr_claim(struct sdio_dev_s *, uint8_t **, struct scr_lease_key *);
int stm32_scr_finish(struct sdio_dev_s *, struct scr_lease_key, int, uint32_t[2]);
int stm32_scr_stop_status(struct sdio_dev_s *, struct scr_lease_key);
#endif
