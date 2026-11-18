from pathlib import Path
import re,subprocess
p=Path(__file__).resolve().parent
cfg=(p.parent/'REFERENCE-SD-CONFIG/candidate.config').read_text()
assert 'CONFIG_IRQCOUNT=y' not in cfg
assert '# CONFIG_ARCH_HIPRI_INTERRUPT is not set' in cfg
assert '# CONFIG_ARMV7M_BASEPRI_WAR is not set' in cfg
h=(p/'include/nuttx/irq.h').read_text()
# Preserve the original nested preprocessor conditions of both API blocks.
def block(name):
 start=h.index('#ifdef CONFIG_IRQCOUNT',h.index('* Name: '+name))
 depth=0
 for m in re.finditer(r'^#\s*(if\w*|endif)\b.*$',h[start:],re.M):
  if m[1].startswith('if'):depth+=1
  else:depth-=1
  if depth==0:return h[start:start+m.end()]+'\n'
 raise AssertionError('unterminated block')
a=(p/'arch/arm/include/armv7-m/irq.h').read_text()
def function(name):
 start=a.index('static always_inline_function',a.index('/* Save the current primask') if name=='up_irq_save' else a.index('/* Restore saved primask'))
 brace=a.index('{',start);end=a.index('\n}',brace)+2
 assert name in a[start:brace]
 return a[start:end]
defines='\n'.join(l for l in (p/'chip.h').read_text().splitlines() if l.startswith('#define NVIC_SYSH_PRIORITY_'))+'\n'+'\n'.join(l for l in (p/'arch/arm/include/armv7-m/nvicpri.h').read_text().splitlines() if l.startswith('#define NVIC_SYSH_'))+'\n'
text=defines+'''#include <stdint.h>
#include <assert.h>
#include <stdio.h>
typedef unsigned irqstate_t;
#define always_inline_function inline
static uint8_t mask;
static uint8_t getbasepri(void){return mask;}
static void setbasepri(uint32_t m){mask=m;}
#define raisebasepri(b) setbasepri(b)
'''+function('up_irq_save')+'\n'+function('up_irq_restore')+'\n'+block('enter_critical_section')+block('leave_critical_section')+'''
int main(void){
for(unsigned initial=0;initial<=0x80;initial+=0x80){
 mask=initial;unsigned a=enter_critical_section();assert(mask==0x80);
 unsigned b=enter_critical_section();assert(mask==0x80);
 leave_critical_section(b);assert(mask==0x80);
 leave_critical_section(a);assert(mask==initial);
}
puts("PASS selected IRQCOUNT-off macros and actual ARM C wrappers, BASEPRI register modeled; not ARM instruction execution");}
'''
(p/'selected.c').write_text(text)
subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=undefined',str(p/'selected.c'),'-o',str(p/'selected')],check=True)
subprocess.run([str(p/'selected')],check=True)
