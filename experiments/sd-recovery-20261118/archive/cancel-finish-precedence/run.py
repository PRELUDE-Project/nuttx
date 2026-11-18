from pathlib import Path
import subprocess,sys
b=Path(__file__).resolve().parent;r=b.parent/'nuttx';f=r/'tools/hosttests/scr_sector'
sys.path.insert(0,str(f));from regenerate import span
src=(r/'arch/arm/src/stm32h7/stm32_sdmmc.c').read_text();a,z=span(src,'stm32_cancel')
prefix=(b.parent/'timeout-isolation-boundary/test.c').read_text().split('\nint main(void){')[0]
test=r'''
int main(void){
 void*m=mmap((void*)0x24000000,4096,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0);assert(m!=MAP_FAILED);
 struct stm32_dev_s*p=m;uint8_t*arena,out[512];struct scr_lease_key key;
 int inputs[]={0,-EBUSY,-EIO,-ETIMEDOUT,-ENOTSUP,-EINVAL};
 for(unsigned sector=0;sector<2;sector++)for(unsigned cause=0;cause<2;cause++)for(unsigned i=0;i<6;i++){
 fresh(p);if(sector){key=begin(p);setup(p);}else{assert(stm32_scr_claim(&p->dev,&arena,&key)==0);p->blocksize=8;assert(stm32_dmarecvsetup(&p->dev,arena,8)==0);}
 p->waitevents=SDIOWAIT_TIMEOUT|SDIOWAIT_ERROR;
 if(cause==0)stm32_eventtimeout((wdparm_t)p);
 assert(stm32_cancel(&p->dev)==-EINPROGRESS);
 int saved=cause==0?-ETIMEDOUT:-ECANCELED;
 assert(p->scrlease.result==saved && p->scrlease.state==SCR_LEASE_QUARANTINED);
 memset(out,0xa5,sizeof out);unsigned notes=notifications;
 int actual=sector?stm32_sector_finish(&p->dev,key,inputs[i],out):stm32_scr_finish(&p->dev,key,inputs[i],(uint32_t*)out);
 int expected=sector||inputs[i]==0||inputs[i]==-EBUSY?saved:inputs[i];
 assert(actual==expected && p->scrlease.result==saved && notifications==notes);
 for(unsigned j=0;j<sizeof out;j++)assert(out[j]==0xa5);
 assert(regs[5]==1);
 printf("sector=%u timeout_before_cancel=%u input=%d return=%d stored=%d\n",sector,cause==0,inputs[i],actual,saved);
 }
 puts("PASS 24 actual cancel/finish error precedence cases; output retained and IDMA not falsely stopped");
 munmap(m,4096);return 0;
}
'''
(b/'test.c').write_text(prefix+'\n\n'+src[a:z]+'\n'+test)
cmd=['cc','-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-parameter','-Wno-unused-variable','-Wno-pointer-to-int-cast','-Wno-return-type','-fsanitize=undefined','-fno-sanitize-recover=all','-I'+str(f/'stubs'),'-I'+str(r/'include/nuttx'),str(b/'test.c'),'-o',str(b/'test')]
subprocess.run(cmd,check=True);v=subprocess.run([str(b/'test')],check=True,capture_output=True,text=True);(b/'result.log').write_text(v.stdout);print(v.stdout)
