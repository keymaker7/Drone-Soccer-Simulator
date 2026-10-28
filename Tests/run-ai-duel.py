"""Actual AI striker goals, own-half retreats, 3-minute session, landing and frozen score."""
import argparse,json,subprocess,uuid,shutil,hashlib
from datetime import datetime,timezone
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--packaged',action='store_true');p.add_argument('--package-root',type=Path,default=ROOT/'dist/Portable/Windows');p.add_argument('--fps',type=int,choices=[20,60,240],default=60);p.add_argument('--cases',type=int,choices=[1,6],default=6);p.add_argument('--capture',action='store_true');a=p.parse_args()
 out=ROOT/'Saved/Verification'/('ai-duel-'+datetime.now(timezone.utc).strftime('%Y%m%d-%H%M%S')+'-'+uuid.uuid4().hex[:8]);out.mkdir(parents=True)
 identity=json.loads(subprocess.check_output(['powershell','-NoProfile','-Command','& "'+str(ROOT/'Scripts/Get-SourceIdentity.ps1')+'" | ConvertTo-Json'],cwd=ROOT,text=True,encoding='utf-8-sig'))
 command=[str(a.package_root/'ImperialWingsSimulator/Binaries/Win64/ImperialWingsSimulator-Win64-Shipping.exe')] if a.packaged else ['E:/Program Files/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe',str(ROOT/'ImperialWingsSimulator.uproject')]
 args=['-game','-unattended','-nosplash','-nosound','-ImperialNoWelcome','-ImperialBridgeUrl=http://127.0.0.1:1','-ImperialAIDuelSmoke','-ImperialAIDuelCases='+str(1 if a.capture else a.cases),'-ImperialDataDir='+str(out/'user'),'-ImperialAIDuelResult='+str(out/'result.json'),'-abslog='+str(out/'runtime.log'),'-UseFixedTimeStep','-FPS='+str(a.fps)]
 args+=['-RenderOffscreen','-ResX=1600','-ResY=1000','-ForceRes','-ImperialAIDuelCapture='+str(out/'duel.png')] if a.capture else ['-nullrhi']
 print(out,flush=True)
 with (out/'console.log').open('w',encoding='utf-8') as log:run=subprocess.run(command+args,cwd=ROOT,stdout=log,stderr=subprocess.STDOUT,timeout=600,creationflags=getattr(subprocess,'CREATE_NO_WINDOW',0))
 r=json.loads((out/'result.json').read_text(encoding='utf-8-sig'));assert run.returncode==0 and r['ok'] and all(c['ok'] for c in r['checks']),[c for c in r['checks'] if not c['ok']]
 assert r['build']['source_sha256']==identity['source_sha256'],'stale build'
 assert abs(r['min_game_dt_s']-1/a.fps)<1e-6 and abs(r['max_game_dt_s']-1/a.fps)<1e-6
 reports=[json.loads(p.read_text(encoding='utf-8-sig')) for p in (out/'user/TrainingResults/AI').glob('*.json')]
 assert len(reports)==(1 if a.capture else a.cases) and all(r['session_mode']=='duel' and r['opponent_goals']>=2 and r['test_session'] and r['stop_reason']=='time_limit' and r['completion_reason']=='landed' for r in reports)
 records=[json.loads(line) for f in (out/'user/FlightLogs').glob('*.jsonl') for line in f.read_text(encoding='utf-8-sig').splitlines() if line.strip()]
 active=[r for r in records if r.get('ai_practice',{}).get('active')]
 assert len(active)>2 and all(r['ai_practice']['duel'] and r['ai_practice']['opponents']==3 and r['ai_practice']['defenders']==2 for r in active),'duel roster telemetry wrong'
 assert any(r['ai_practice']['attack_cycles']>=1 for r in active) and any(r['ai_practice']['landing'] for r in active),'missing attack return / landing trace'
 if not a.capture:
  checks=[]
  def mini(name,user,flag):
   folder=out/name;folder.mkdir();user.mkdir(parents=True,exist_ok=True)
   extra=[x for x in args if not x.startswith(('-ImperialDataDir=','-ImperialAIDuelResult=','-abslog=','-ImperialAIDuelCases='))]+['-ImperialDataDir='+str(user),'-ImperialAIDuelResult='+str(folder/'result.json'),'-abslog='+str(folder/'runtime.log'),'-ImperialAIDuelCases=1',flag]
   with (folder/'console.log').open('w',encoding='utf-8') as log:proc=subprocess.run(command+extra,cwd=ROOT,stdout=log,stderr=subprocess.STDOUT,timeout=180,creationflags=getattr(subprocess,'CREATE_NO_WINDOW',0))
   sub=json.loads((folder/'result.json').read_text(encoding='utf-8-sig'));assert proc.returncode==0 and sub['ok'],[c for c in sub['checks'] if not c['ok']];assert sub['build']['source_sha256']==identity['source_sha256'];checks.append(dict(name=name,result=sub));print('PASS',name,len(sub['checks']),'checks',flush=True)
  mini('early_stop',out/'interrupt-user','-ImperialAIDuelInterrupted')
  interrupted=[json.loads(p.read_text(encoding='utf-8-sig')) for p in (out/'interrupt-user/TrainingResults/AI').glob('*.json')];assert len(interrupted)==1 and interrupted[0]['stop_reason']=='manual_stop' and interrupted[0]['completion_reason']=='landed'
  for count in sorted({v['opponents'] for v in reports}):
   source=next(p for p in (out/'user/TrainingResults/AI').glob('*.json') if json.loads(p.read_text(encoding='utf-8-sig'))['opponents']==count);user=out/('reload-'+str(count)+'-user');dest=user/'TrainingResults/AI'/source.name;dest.parent.mkdir(parents=True);shutil.copy2(source,dest);before=dest.read_bytes();mini('reload-'+str(count),user,'-ImperialAIDuelReload');assert before==dest.read_bytes()
  (out/'additional.json').write_text(json.dumps(dict(ok=True,cases=checks),indent=2)+'\n',encoding='utf-8')
 assert not a.capture or (out/'duel.png').is_file()
 print('PASS',len(r['checks']),'native checks;',out/'result.json',flush=True)
if __name__=='__main__':main()
