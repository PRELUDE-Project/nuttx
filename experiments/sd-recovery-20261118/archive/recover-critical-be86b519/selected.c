#define NVIC_SYSH_PRIORITY_MIN     0xf0 /* All bits set in minimum priority */
#define NVIC_SYSH_PRIORITY_DEFAULT 0x80 /* Midpoint is the default */
#define NVIC_SYSH_PRIORITY_MAX     0x00 /* Zero is maximum priority */
#define NVIC_SYSH_PRIORITY_STEP    0x10 /* Four bits of interrupt priority used */
#define NVIC_SYSH_MAXNORMAL_PRIORITY  NVIC_SYSH_PRIORITY_DEFAULT
#define NVIC_SYSH_HIGH_PRIORITY       (NVIC_SYSH_PRIORITY_DEFAULT - 2*NVIC_SYSH_PRIORITY_STEP)
#define NVIC_SYSH_DISABLE_PRIORITY    NVIC_SYSH_PRIORITY_DEFAULT
#define NVIC_SYSH_SVCALL_PRIORITY     (NVIC_SYSH_PRIORITY_DEFAULT - 1*NVIC_SYSH_PRIORITY_STEP)
#include <stdint.h>
#include <assert.h>
#include <stdio.h>
typedef unsigned irqstate_t;
#define always_inline_function inline
static uint8_t mask;
static uint8_t getbasepri(void){return mask;}
static void setbasepri(uint32_t m){mask=m;}
#define raisebasepri(b) setbasepri(b)
static always_inline_function irqstate_t up_irq_save(void)
{
  /* Probably raising priority */

  uint8_t basepri = getbasepri();
  raisebasepri(NVIC_SYSH_DISABLE_PRIORITY);
  return (irqstate_t)basepri;
}
static always_inline_function void up_irq_restore(irqstate_t flags)
{
  /* In this case, we are always retaining or lowering the priority value */

  setbasepri((uint32_t)flags);
}
#ifdef CONFIG_IRQCOUNT
#  if CONFIG_SCHED_CRITMONITOR_MAXTIME_CSECTION >= 0 || \
      defined(CONFIG_SCHED_INSTRUMENTATION_CSECTION)
irqstate_t enter_critical_section(void) noinstrument_function;
#  else
#    define enter_critical_section() enter_critical_section_wo_note()
#  endif

irqstate_t enter_critical_section_wo_note(void) noinstrument_function;
#else
#  define enter_critical_section() up_irq_save()
#  define enter_critical_section_wo_note() up_irq_save()
#endif
#ifdef CONFIG_IRQCOUNT
#  if CONFIG_SCHED_CRITMONITOR_MAXTIME_CSECTION >= 0 || \
      defined(CONFIG_SCHED_INSTRUMENTATION_CSECTION)
void leave_critical_section(irqstate_t flags) noinstrument_function;
#  else
#    define leave_critical_section(f) leave_critical_section_wo_note(f)
#  endif

void leave_critical_section_wo_note(irqstate_t flags) noinstrument_function;
#else
#  define leave_critical_section(f) up_irq_restore(f)
#  define leave_critical_section_wo_note(f) up_irq_restore(f)
#endif

int main(void){
for(unsigned initial=0;initial<=0x80;initial+=0x80){
 mask=initial;unsigned a=enter_critical_section();assert(mask==0x80);
 unsigned b=enter_critical_section();assert(mask==0x80);
 leave_critical_section(b);assert(mask==0x80);
 leave_critical_section(a);assert(mask==initial);
}
puts("PASS selected IRQCOUNT-off macros and actual ARM C wrappers, BASEPRI register modeled; not ARM instruction execution");}
