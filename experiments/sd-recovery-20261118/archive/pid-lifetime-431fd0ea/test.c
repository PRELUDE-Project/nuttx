#define main original_main
#include "${RECOVERY_ROOT}/scr-claim-null-guard/product/tools/hosttests/scr_sector/driver.c"
#undef main

int main(void){void*m=mmap((void*)0x24000000,4096,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0);assert(m!=MAP_FAILED);struct stm32_dev_s*p=m;uint8_t out[512];unsigned count=0;
for(unsigned closed=0;closed<2;closed++){
fresh(p);struct scr_lease_key key=begin(p);setup(p);command(p);memset(out,0xa5,sizeof out);
current_owner=2;assert(stm32_sector_finish(&p->dev,key,0,out)==-ESTALE);
if(closed){p->waitevents=SDIOWAIT_TIMEOUT;stm32_endwait(p,SDIOWAIT_TIMEOUT);assert(p->scrlease.state==SCR_LEASE_QUARANTINED);}
/* Simulate numeric task-ID reuse only. There is no real task deletion here. */
current_owner=key.owner;assert(scr_lease_matches(&p->scrlease,key));
if(closed){terminal(p,0);assert(stm32_sector_finish(&p->dev,key,0,out)==-ETIMEDOUT);for(unsigned i=0;i<512;i++)assert(out[i]==0xa5);assert(p->scrlease.state==SCR_LEASE_QUARANTINED);uint8_t*a;struct scr_lease_key other;assert(stm32_scr_claim(&p->dev,&a,&other)==-EBUSY);}
else{memset(p->scrlease.arena,0x5a,512);terminal(p,0);assert(stm32_sector_finish(&p->dev,key,0,out)==0);for(unsigned i=0;i<512;i++)assert(out[i]==0x5a);assert(p->scrlease.state==SCR_LEASE_RETIRED);}
count++;
}printf("PASS %u numeric-ID models: quarantine survives ID reuse; ACTIVE saved-key continuation cannot distinguish a reincarnated numeric owner. Not an OS deletion/reuse test.\n",count);return 0;}
