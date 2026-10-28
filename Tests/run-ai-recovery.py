"""Real collision and controlled obstruction fixtures; no real radio or entanglement claim."""
import argparse,json,subprocess,uuid,shutil,hashlib
from datetime import datetime,timezone
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def read(p):return json.loads(p.read_text(encoding='utf-8-sig'))
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--packaged',action='store_true');p.add_argument('--package-root',type=Path,default=ROOT/'dist/Portable/Windows');p.add_argument('--fps',type=int,choices=[20,60,240],default=60);p.add_argument('--capture',action='store_true');p.add_argument('--engine',type=Path,default=Path('E:/Program Files/UE_5.8'));a=p.parse_args()
 out=ROOT/'Saved/Verification'/('ai-recovery-'+datetime.now(timezone.utc).strftime('%Y%m%d-%H%M%S')+'-'+uuid.uuid4().hex[:8]);out.mkdir(parents=True)
 identity=json.loads(subprocess.check_output(['powershell','-NoProfile','-Command','& "'+str(ROOT/'Scripts/Get-SourceIdentity.ps1')+'" | ConvertTo-Json'],cwd=ROOT,text=True,encoding='utf-8-sig'))
 command=[str(a.package_root/'ImperialWingsSimulator/Binaries/Win64/ImperialWingsSimulator-Win64-Shipping.exe')] if a.packaged else [str(a.engine/'Engine/Binaries/Win64/UnrealEditor-Cmd.exe'),str(ROOT/'ImperialWingsSimulator.uproject')]
 results=[]
 def run(name,data,extra=[]):
  folder=out/name;folder.mkdir();data.mkdir(parents=True,exist_ok=True)
  args=['-game','-unattended','-nosplash','-nosound','-ImperialNoWelcome','-ImperialBridgeUrl=http://127.0.0.1:1','-ImperialAIRecoverySmoke','-ImperialDataDir='+str(data),'-ImperialAIRecoveryResult='+str(folder/'result.json'),'-abslog='+str(folder/'runtime.log'),'-UseFixedTimeStep','-FPS='+str(a.fps)]+extra
  args+=['-RenderOffscreen','-ResX=1600','-ResY=1000','-ForceRes','-ImperialAIRecoveryCapture='+str(folder/'recovery.png')] if a.capture else ['-nullrhi']
  with (folder/'console.log').open('w',encoding='utf-8') as log:
   proc=subprocess.run(command+args,cwd=ROOT,stdout=log,stderr=subprocess.STDOUT,timeout=240,creationflags=getattr(subprocess,'CREATE_NO_WINDOW',0))
  r=read(folder/'result.json');assert proc.returncode==0 and r['ok'] and all(c['ok'] for c in r['checks']),[(c['case'],c['check'],c['value']) for c in r['checks'] if not c['ok']]
  assert r['build']['source_sha256']==identity['source_sha256'],'source mismatch'
  assert abs(r['min_game_dt_s']-1/a.fps)<1e-6 and abs(r['max_game_dt_s']-1/a.fps)<1e-6,'actual frame interval mismatch'
  results.append(dict(name=name,path=(folder/'result.json').relative_to(ROOT).as_posix(),sha256=hashlib.sha256((folder/'result.json').read_bytes()).hexdigest(),result=r));print('PASS',name,len(r['checks']),'checks',flush=True)
 print(out,flush=True);data=out/'user';run('flight',data)
 reports=[(f,read(f)) for f in (data/'TrainingResults/AI').glob('*.json')]
 assert len(reports)==(3 if a.capture else 5) and all(r['completion_reason']=='landed' and r['test_session'] for _,r in reports)
 if not a.capture:
  for reason in ['recovery_failed','defender_grounded','defender_unavailable']:
   source=next(f for f,r in reports if r['stop_reason']==reason);user=out/('reload-'+reason+'-user');dest=user/'TrainingResults/AI'/source.name;dest.parent.mkdir(parents=True);shutil.copy2(source,dest)
   before=dest.read_bytes();run('reload-'+reason,user,['-ImperialAIRecoveryReload','-ImperialAIRecoveryExpected='+reason]);assert dest.read_bytes()==before
 else:assert (out/'flight/recovery.png').is_file()
 (out/'results.json').write_text(json.dumps(dict(ok=True,build=identity,cases=results,scope='Physics collision and explicit constrained/inverted/disabled diagnostic fixtures. No real USB.'),indent=2)+'\n',encoding='utf-8')
 print(out/'results.json',flush=True)
if __name__=='__main__':main()
