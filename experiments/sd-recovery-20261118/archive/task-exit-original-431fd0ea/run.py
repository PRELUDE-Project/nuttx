from pathlib import Path
import subprocess
b=Path(__file__).resolve().parent
notify=(b/'sched/task/task_cancelpt.c').read_text().split('bool nxnotify_cancellation',1)[1]
recover=(b/'sched/wdog/wd_recover.c').read_text().split('void wd_recover',1)[1]
s='''#include <stdbool.h>
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
'''+ 'bool nxnotify_cancellation'+notify+'\nvoid wd_recover'+recover+'''
int main(void){unsigned count=0;for(unsigned disable=0;disable<2;disable++)for(unsigned force=0;force<2;force++){
struct tcb_s t={.flags=force,.tls={.tl_cpstate=disable},.waitdog=true};
bool driver_waitwdog=true;bool r=nxnotify_cancellation(&t);
assert(r==(disable&&!force));assert(!!(t.tls.tl_cpstate&CANCEL_FLAG_CANCEL_PENDING)==r);assert(depth==0);
wd_recover(&t);assert(!t.waitdog&&driver_waitwdog);count++;
}printf("PASS %u fixed-source cancel notification / TCB watchdog models; CONFIG_CANCELLATION_POINTS off. Forced bypass and independent driver watchdog preserved. Not OS dispatch.\\n",count);}
'''
(b/'test.c').write_text(s)
subprocess.run(['cc','-std=c11','-Wall','-Wextra','-fsanitize=undefined','-fno-sanitize-recover=all',str(b/'test.c'),'-o',str(b/'test')],check=True)
p=subprocess.run([str(b/'test')],capture_output=True,text=True);(b/'result.log').write_text(p.stdout+p.stderr);print(p.stdout+p.stderr);assert p.returncode==0
