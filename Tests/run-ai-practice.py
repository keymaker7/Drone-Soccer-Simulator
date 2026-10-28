"""Six native UI / Chaos defensive practice scenarios with isolated user data."""
import argparse,json,subprocess,uuid
from pathlib import Path
from datetime import datetime,timezone
ROOT=Path(__file__).resolve().parents[1]
def main():
 p=argparse.ArgumentParser(description=__doc__)
 p.add_argument('--packaged',action='store_true');p.add_argument('--package-root',type=Path,default=ROOT/'dist/Portable/Windows')
 p.add_argument('--engine',type=Path,default=Path('E:/Program Files/UE_5.8'));p.add_argument('--fps',type=int,choices=[20,30,60,120,240],default=60)
 p.add_argument('--capture',action='store_true');a=p.parse_args()
 out=ROOT/'Saved/Verification'/('ai-practice-'+datetime.now(timezone.utc).strftime('%Y%m%d-%H%M%S')+'-'+uuid.uuid4().hex[:8]);out.mkdir(parents=True)
 identity=json.loads(subprocess.check_output(['powershell','-NoProfile','-Command','& "'+str(ROOT/'Scripts/Get-SourceIdentity.ps1')+'" | ConvertTo-Json'],cwd=ROOT,text=True,encoding='utf-8-sig'))
 command=[str(a.package_root/'ImperialWingsSimulator/Binaries/Win64/ImperialWingsSimulator-Win64-Shipping.exe')] if a.packaged else [str(a.engine/'Engine/Binaries/Win64/UnrealEditor-Cmd.exe'),str(ROOT/'ImperialWingsSimulator.uproject')]
 args=['-game','-nosound','-nosplash','-unattended','-ImperialNoWelcome','-ImperialBridgeUrl=http://127.0.0.1:1','-ImperialDataDir='+str(out/'user'),'-ImperialAIPracticeSmoke','-ImperialAIPracticeResult='+str(out/'result.json'),'-abslog='+str(out/'runtime.log'),'-UseFixedTimeStep','-FPS='+str(a.fps)]
 args+=['-RenderOffscreen','-ResX=1600','-ResY=1000','-ForceRes','-ImperialAIPracticeCapture='+str(out/'ai-practice.png')] if a.capture else ['-nullrhi']
 print(out,flush=True)
 with (out/'console.log').open('w',encoding='utf-8') as log:
  run=subprocess.run(command+args,cwd=ROOT,stdout=log,stderr=subprocess.STDOUT,timeout=420,creationflags=getattr(subprocess,'CREATE_NO_WINDOW',0))
 r=json.loads((out/'result.json').read_text(encoding='utf-8-sig'))
 assert run.returncode==0 and r['ok'] and all(c['ok'] for c in r['checks']),[c for c in r['checks'] if not c['ok']]
 assert r['build']['source_sha256']==identity['source_sha256'],'stale build'
 assert abs(r['min_game_dt_s']-1/a.fps)<1e-6 and abs(r['max_game_dt_s']-1/a.fps)<1e-6,'actual fixed timestep mismatch'
 assert not a.capture or (out/'ai-practice.png').is_file(),'missing GPU capture'
 records=[json.loads(line) for f in (out/'user/FlightLogs').glob('*.jsonl') for line in f.read_text(encoding='utf-8-sig').splitlines() if line.strip()]
 active=[r for r in records if r.get('ai_practice',{}).get('active')]
 assert len(active)>2,'missing AI flight telemetry'
 assert all(r['ai_practice']['seed']==1 and r['ai_practice']['defenders']==3 and r['source_sha256']==identity['source_sha256'] for r in active),'AI record metadata mismatch'
 assert any(r['ai_practice']['landing'] for r in active),'landing state not recorded'
 assert all(len(d['autonomous_target_m'])==3 for r in active for d in r['aircraft']),'target trajectories absent'
 print('PASS',len(r['checks']),'checks,',r['completed_flight_scenarios'],'real defensive flights and landings;',out/'result.json',flush=True)
if __name__=='__main__':main()
