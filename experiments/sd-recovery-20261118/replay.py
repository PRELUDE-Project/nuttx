#!/usr/bin/env python3
"""Replay archived host proposals, never flash or modify the source tree."""
from pathlib import Path
import argparse,subprocess,tempfile,json
p=argparse.ArgumentParser();p.add_argument('--repo',type=Path,required=True);a=p.parse_args()
here=Path(__file__).resolve().parent;repo=a.repo.resolve();fixture=repo/'tools/hosttests/scr_sector'
flags=['-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-parameter','-Wno-unused-variable','-Wno-pointer-to-int-cast','-Wno-sign-compare','-fsanitize=undefined','-fno-sanitize-recover=all','-I'+str(fixture),'-I'+str(fixture/'stubs'),'-I'+str(repo/'include/nuttx')]
cases=[]
for name in ['recover-atomic-binding','recover-export-integration','recover-finish-tcb-scope','recover-fixed-hook','recover-key-binding','recover-os-source-candidate','recover-seal-callgraph','recover-tcb-scope']:
 cases.append((name,[here/'archive'/(name+'-be86b519')/'connected.c'],[]))
for kind in ['baseline','candidate','baseline-generation','candidate-generation']:
 cases.append(('preflight-'+kind,[here/'archive/recover-finish-preflight-be86b519'/(kind+'.c')],[]))
for kind in ['baseline','candidate']:
 cases.append(('reset-'+kind,[here/'archive/recover-reset-connected-be86b519'/(kind+'.c')],[]))
split=here/'archive/recover-split-host-be86b519';cases.append(('split-host',[split/'driver-host.c',split/'scheduler.c'],['-DHOST_RECOVERY_TEST']))
results=[]
with tempfile.TemporaryDirectory(prefix='sd-recovery-host-') as tmp:
 for name,src,extra in cases:
  out=Path(tmp)/name
  c=subprocess.run(['cc',*flags,*extra,*map(str,src),'-o',str(out)],text=True,capture_output=True)
  if c.returncode:raise SystemExit(name+' compile failed\n'+c.stderr)
  t=subprocess.run([str(out)],text=True,capture_output=True)
  if t.returncode:raise SystemExit(name+' run failed\n'+t.stderr)
  results.append({'case':name,'result':'PASS','stdout':t.stdout.strip()})
print(json.dumps({'scope':'host UBSan; not target/OS/hardware','cases':results},indent=2))
