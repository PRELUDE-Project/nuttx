from pathlib import Path
import subprocess,tempfile,json,argparse
parser=argparse.ArgumentParser();parser.add_argument('--repo',type=Path,required=True);args=parser.parse_args()
p=Path(__file__).resolve().parent;r=args.repo.resolve();f=r/'tools/hosttests/scr_sector'
# Reject mismatched input before compiling; these are source pins, not a target-build claim.
import hashlib

def gitblob(data):
 return hashlib.sha1(b'blob '+str(len(data)).encode()+b'\0'+data).hexdigest()
for pin in json.loads((p/'source-pins.json').read_text()):
 data=(r/pin['path']).read_bytes()
 if gitblob(data)!=pin['sha']:
  raise SystemExit('Pinned dependency mismatch: '+pin['path'])
original=(p/'sched_releasetcb-original.c').read_bytes()
if gitblob(original)!='52ce5f3159e3ddd9fd0fe29025a4b3c321415b86':
 raise SystemExit('Pinned release original mismatch')
original_body=original.decode().split('int nxsched_release_tcb(',1)[1]
connected=(p/'connected.c').read_text().split('int nxsched_release_tcb(',1)[1].split('\nint main(void)',1)[0]
hook='#ifdef PROPOSED_RELEASE_HOOK\n      recover_notices(tcb);\n#endif\n'
if connected.count(hook)!=1 or connected.replace(hook,'').strip()!=original_body.strip():
 raise SystemExit('Release extraction mismatch beyond the one proposed hook')
print('PASS fixed original blob, three fixed repo dependencies and exact release-body extraction')
required=['CONFIG_SD_SECTOR_READ_REVIEW','CONFIG_ARCH_CHIP_STM32H7','CONFIG_STM32H7_SDMMC1','CONFIG_STM32H7_SDMMC_IDMA','CONFIG_SD_SCR_CONTAINMENT_REVIEW','CONFIG_SD_RECEIVE_LEASE_ONLY_REVIEW']
unsupported=['CONFIG_STM32H7_SDMMC2','CONFIG_SMP','CONFIG_ARCH_HIPRI_INTERRUPT','CONFIG_SCHED_TICKLESS','CONFIG_SCHED_INSTRUMENTATION','CONFIG_IRQCOUNT']
flags=['-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-parameter','-Wno-unused-variable','-Wno-pointer-to-int-cast','-Wno-sign-compare']
def defs(xs):return ['-D'+x+'=1' for x in xs]
with tempfile.TemporaryDirectory() as tmp:
 cfg=Path(tmp)/'include/nuttx';cfg.mkdir(parents=True)
 (cfg/'config.h').write_text(''.join('#define '+x+' 1\n' for x in required))
 obj=str(Path(tmp)/'scheduler.o')
 subprocess.run(['cc',*flags,'-I'+str(cfg.parent),'-I'+str(p/'stubs'),'-c',str(p/'scheduler.c'),'-o',obj],check=True)
 symbols=subprocess.run(['nm',obj],check=True,capture_output=True,text=True).stdout
 assert ' T recover_notices' in symbols and ' U sd_recovery_notify' in symbols
 assert 'recovery_lock_held' not in symbols and 'recovery_slot_for_test' not in symbols
 print('PASS generated test config header without -D CONFIG; real module and mandatory hook present, no host-only lock API')
 for case in ['baseline','candidate']:
  out=str(Path(tmp)/case)
  subprocess.run(['cc',*(['-DPROPOSED_RELEASE_HOOK'] if case=='candidate' else []),*flags,*defs(required),'-DHOST_RECOVERY_TEST','-fsanitize=undefined','-fno-sanitize-recover=all','-I'+str(p/'stubs'),'-I'+str(f),'-I'+str(f/'stubs'),'-I'+str(r/'include/nuttx'),str(p/'scheduler.c'),str(p/'connected.c'),'-o',out],check=True)
  subprocess.run([out],check=True)
  print(case, 'PASS: release ordering, prior-recover deduplication, critical restored; host only')
