"""Bounded actual Unreal/APF integration test. Does not create HID input.

The test-only HTTP handler injects a 503 outage into real upstream APF traffic.
Only its own child process is terminated on timeout. No persistent service.
"""
import argparse
from datetime import datetime, timezone
import json
from pathlib import Path
import subprocess
import sys
import threading
import time
from http.server import ThreadingHTTPServer

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'Bridge'))
from sdacs_bridge import GuidanceBridge, make_handler

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repo',required=True)
    parser.add_argument('--engine',default='E:/Program Files/UE_5.8')
    parser.add_argument('--executable')
    args=parser.parse_args()
    output=ROOT/'Saved/Verification'/('autonomy-'+datetime.now(timezone.utc).strftime('%Y%m%d-%H%M%S-%f'))
    output.mkdir(parents=True)
    result=output/'result.json'
    events=Path(str(result)+'.events.jsonl')
    class TrackingBridge(GuidanceBridge):
        last_trace=0
        def step(self,payload):
            response=super().step(payload)
            if time.monotonic()-self.last_trace>1:
                self.last_trace=time.monotonic()
                with (output/'planner-trace.jsonl').open('a',encoding='utf-8') as trace:
                    trace.write(json.dumps({'request':payload,'response':response})+'\n')
            return response
    bridge=TrackingBridge(args.repo)
    handler=make_handler(bridge)
    failure_until=0
    class OutageHandler(handler):
        def do_POST(self):
            if time.monotonic()<failure_until:
                self._reply(503,{'error':'explicit bounded integration-test outage'})
                return
            super().do_POST()
    server=ThreadingHTTPServer(('127.0.0.1',0),OutageHandler)
    server.daemon_threads=True
    thread=threading.Thread(target=server.serve_forever,daemon=True)
    thread.start()
    if args.executable:
        command=[args.executable]
    else:
        command=[str(Path(args.engine)/'Engine/Binaries/Win64/UnrealEditor-Cmd.exe'),str(ROOT/'ImperialWingsSimulator.uproject')]
    command+=['-game','-nullrhi','-nosound','-unattended','-nosplash','-ImperialNoWelcome','-ImperialSwarmSmoke',
              '-ImperialBridgeUrl=http://127.0.0.1:'+str(server.server_port),'-ImperialResult='+str(result),
              '-ImperialDataDir='+str(output/'userdata'),'-abslog='+str(output/'unreal.log')]
    seen=0
    interrupted=False
    started=time.monotonic()
    try:
        with (output/'console.log').open('w',encoding='utf-8') as log:
            child=subprocess.Popen(command,stdout=log,stderr=subprocess.STDOUT)
            try:
                while child.poll() is None:
                    if time.monotonic()-started>165:raise TimeoutError('Bounded autonomy test exceeded 165 seconds')
                    if events.exists():
                        # Unreal holds a short exclusive write handle while
                        # appending. Retry next poll without losing our cursor.
                        try:lines=events.read_text(encoding='utf-8-sig').splitlines()
                        except PermissionError:
                            time.sleep(.03)
                            continue
                        for line in lines[seen:]:
                            try:row=json.loads(line)
                            except json.JSONDecodeError:break
                            seen+=1
                            if row['event']=='sample':continue
                            print(row['event'],round(row['game_time_s'],2),row['mode'],flush=True)
                            if row['event']=='interrupt_return' and not interrupted:
                                interrupted=True;failure_until=time.monotonic()+3.8
                    time.sleep(.1)
                exit_code=child.returncode
            finally:
                if child.poll() is None:child.terminate();child.wait(timeout=10)
    finally:
        server.shutdown();server.server_close();thread.join(timeout=3)
    if not result.exists():raise RuntimeError('Unreal did not produce a result: '+str(output))
    data=json.loads(result.read_text(encoding='utf-8-sig'))
    data['test_host']={'injected_http_503':interrupted,'upstream':bridge.health(),'child_exit':exit_code,'wall_seconds':round(time.monotonic()-started,3)}
    result.write_text(json.dumps(data,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print('Autonomy feedback:',result,flush=True)
    for key in ['ok','ten_airborne','ten_landed_disarmed','minimum_separation_m','hold_max_drift_cm','timeout_local_hold','reconnect_does_not_resume','manual_match_auto_rejected']:
        print(key,data.get(key),flush=True)
    return 0 if exit_code==0 and data['ok'] and interrupted else 1

if __name__=='__main__':raise SystemExit(main())
