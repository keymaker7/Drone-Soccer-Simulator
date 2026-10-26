"""Bounded offscreen GPU preview of ten actual APF/Chaos aircraft, not HID input."""
import argparse
from datetime import datetime,timezone
from http.server import ThreadingHTTPServer
import json
from pathlib import Path
import subprocess
import sys
import threading
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'Bridge'))
from sdacs_bridge import GuidanceBridge,make_handler

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repo',required=True)
    parser.add_argument('--executable',required=True)
    args=parser.parse_args()
    output=ROOT/'Saved/Verification'/('render-autonomy-'+datetime.now(timezone.utc).strftime('%Y%m%d-%H%M%S-%f'))
    output.mkdir(parents=True)
    bridge=GuidanceBridge(args.repo)
    server=ThreadingHTTPServer(('127.0.0.1',0),make_handler(bridge));server.daemon_threads=True
    thread=threading.Thread(target=server.serve_forever,daemon=True);thread.start()
    command=[args.executable,'-RenderOffscreen','-windowed','-ResX=1600','-ResY=1000','-ForceRes','-nosound','-unattended','-NoScreenMessages',
             '-ImperialNoWelcome','-ImperialDemo','-ImperialView=3','-ImperialVisualQuality=High','-ImperialProfile',
             '-ImperialCaptureTime=19','-ImperialCapture='+str(output/'autonomy.png'),'-ImperialDataDir='+str(output/'userdata'),
             '-ImperialBridgeUrl=http://127.0.0.1:'+str(server.server_port),'-abslog='+str(output/'unreal.log')]
    try:
        with (output/'console.log').open('w',encoding='utf-8') as log:
            child=subprocess.Popen(command,stdout=log,stderr=subprocess.STDOUT)
            try:code=child.wait(timeout=65)
            finally:
                if child.poll() is None:child.terminate();child.wait(timeout=10)
    finally:server.shutdown();server.server_close();thread.join(timeout=3)
    profile=json.loads((output/'userdata/render-profile.json').read_text(encoding='utf-8-sig'))
    if code or not (output/'autonomy.png').is_file() or bridge.requests<30:raise RuntimeError('Rendered APF preview did not complete')
    (output/'upstream.json').write_text(json.dumps(bridge.health(),indent=2),encoding='utf-8')
    print('Actual ten-aircraft GPU preview:',output)
    print(json.dumps({k:profile[k] for k in ['samples','average_frame_ms','p95_frame_ms','average_fps','res_x','res_y']}))
    return 0
if __name__=='__main__':raise SystemExit(main())
