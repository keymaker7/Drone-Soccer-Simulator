"""Read real Windows WinMM joystick axes. Never arms or writes calibration."""
from __future__ import annotations
import argparse
import ctypes as c
import json
import pathlib
import sys
import time
from ctypes import wintypes as w

AXES = ("X", "Y", "Z", "R", "U", "V")
class Caps(c.Structure):
    _fields_ = [("mid",w.WORD),("pid",w.WORD),("name",w.WCHAR*32)]+[(n,w.UINT) for n in ("xmin","xmax","ymin","ymax","zmin","zmax","buttons","periodmin","periodmax","rmin","rmax","umin","umax","vmin","vmax","caps","maxaxes","axes","maxbuttons")]+[("regkey",w.WCHAR*32),("oem",w.WCHAR*260)]
class Position(c.Structure):
    _fields_ = [(n,w.DWORD) for n in ("size","flags","x","y","z","r","u","v","buttons","button_number","pov","reserved1","reserved2")]

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--duration",type=float,default=1)
    ap.add_argument("--output",type=pathlib.Path)
    ap.add_argument("--timeline",type=pathlib.Path,help="Optional local timestamped raw input journal")
    args=ap.parse_args()
    if sys.platform!="win32": raise SystemExit("Windows is required.")
    if not 0<=args.duration<=3600: raise SystemExit("Duration must be 0..3600 seconds.")
    dll=c.WinDLL("winmm")
    dll.joyGetDevCapsW.argtypes=[c.c_size_t,c.POINTER(Caps),w.UINT]
    dll.joyGetPosEx.argtypes=[w.UINT,c.POINTER(Position)]
    found={}
    last_write=-1.0
    started=time.monotonic()
    while True:
        for slot in range(min(16,dll.joyGetNumDevs())):
            caps=Caps(); pos=Position();pos.size=c.sizeof(pos);pos.flags=255
            if dll.joyGetDevCapsW(slot,c.byref(caps),c.sizeof(caps)) or dll.joyGetPosEx(slot,c.byref(pos)): continue
            ranges=[(getattr(caps,a.lower()+"min"),getattr(caps,a.lower()+"max")) for a in AXES]
            raw=[getattr(pos,a.lower()) for a in AXES]
            # Some drivers return a centred initialization packet before their
            # first real report. Exclude that startup period from observed travel.
            if time.monotonic()-started<.25: continue
            device=found.setdefault(slot,{"slot":slot,"name":caps.name,"mid":f"{caps.mid:04X}","pid":f"{caps.pid:04X}","axis_count":caps.axes,"buttons":caps.buttons,"ranges":ranges,"samples":0,"minimum":raw.copy(),"maximum":raw.copy(),"last":raw.copy(),"buttons_seen":0})
            device["samples"]+=1;device["last"]=raw
            device["minimum"]=[min(a,b) for a,b in zip(device["minimum"],raw)]
            device["maximum"]=[max(a,b) for a,b in zip(device["maximum"],raw)]
            device["buttons_seen"]|=pos.buttons
        elapsed=time.monotonic()-started
        result={"elapsed_s":round(elapsed,3),"source":"actual WinMM joyGetPosEx","devices":list(found.values())}
        for dev in result["devices"]:
            dev["moved_axes"]=[a for a,lo,hi,r in zip(AXES,dev["minimum"],dev["maximum"],dev["ranges"]) if hi-lo>max(100,(r[1]-r[0])*.05)]
            dev["normalized_last"]=[round((v-r[0])/max(1,r[1]-r[0]),4) for v,r in zip(dev["last"],dev["ranges"])]
        if args.output and (elapsed-last_write>=.25 or elapsed>=args.duration):
            args.output.parent.mkdir(parents=True,exist_ok=True)
            args.output.write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding="utf-8")
            if args.timeline:
                args.timeline.parent.mkdir(parents=True,exist_ok=True)
                with args.timeline.open("a",encoding="utf-8") as journal:
                    journal.write(json.dumps({"elapsed_s":round(elapsed,3),"devices":[{"slot":d["slot"],"mid":d["mid"],"pid":d["pid"],"raw":d["last"]} for d in found.values()]})+"\n")
            last_write=elapsed
        if elapsed>=args.duration: break
        time.sleep(.02)
    print(json.dumps(result,ensure_ascii=False,indent=2))
if __name__=="__main__":main()
