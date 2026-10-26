"""Summarize actual UE flight recordings; never generates or replays USB input."""
import argparse
import json
import math
from pathlib import Path


def analyze(records):
    if not records:
        raise ValueError('Recording is empty')
    selected=int(records[0]['selected'])
    fingerprint=records[0]['source_sha256']
    previous=None
    samples=[]
    for row in records:
        wall=float(row['wall_s'])
        if not math.isfinite(wall) or previous is not None and wall<previous:
            raise ValueError('Nonfinite or backwards recording time')
        if row['source_sha256']!=fingerprint:
            raise ValueError('Recording mixes source builds')
        previous=wall
        drone=next((a for a in row['aircraft'] if a['id']==selected),None)
        if drone is None:
            raise ValueError('Selected aircraft is missing')
        z=float(drone['position_m'][2])
        speed=float(drone['speed_mps'])
        throttle=float(drone['throttle'])
        tilt=float(drone['tilt_deg'])
        if not all(math.isfinite(v) for v in (z,speed,throttle,tilt)):
            raise ValueError('Invalid body state')
        usb=row['radio_connected'] and row['radio_enabled'] and drone['input_source']=='USB' and not drone['autonomous'] and drone['flight_mode']=='ANGLE'
        samples.append(dict(time=wall,z=z,speed=speed,throttle=throttle,tilt=tilt,usb=usb,armed=bool(drone['armed'])))
    airborne_s=hover_s=longest_airborne=longest_hover=0.0
    first_airborne=None
    landed=False
    interrupted=False
    for index,sample in enumerate(samples):
        gap=sample['time']-samples[index-1]['time'] if index else 0
        continuous=0<=gap<=.15
        flying=sample['usb'] and sample['armed'] and sample['z']>.45
        hovering=flying and sample['z']<2.0 and sample['speed']<.35 and sample['tilt']<7
        airborne_s=airborne_s+gap if flying and continuous else 0
        hover_s=hover_s+gap if hovering and continuous else 0
        longest_airborne=max(longest_airborne,airborne_s)
        longest_hover=max(longest_hover,hover_s)
        if flying and first_airborne is None:
            first_airborne=index
        if first_airborne is not None and not landed:
            if not sample['usb'] or not continuous:
                interrupted=True
            if sample['usb'] and sample['armed'] and sample['z']<.35 and sample['speed']<.25 and sample['throttle']<.1:
                landed=not interrupted
    takeoff=longest_airborne>=1.5
    result=dict(source_sha256=fingerprint,aircraft=selected+1,samples=len(samples),
                usb_takeoff_observed=takeoff,usb_hover_observed=longest_hover>=1.5,
                uninterrupted_usb_landing_observed=landed,final_disarmed=not samples[-1]['armed'],
                longest_usb_airborne_s=round(longest_airborne,3),longest_usb_hover_s=round(longest_hover,3),
                max_usb_altitude_m=max((s['z'] for s in samples if s['usb']),default=None),
                radio_or_sampling_interrupted=interrupted,
                note='Game-state evidence. This does not measure USB-to-screen latency or authenticate hardware outside the recorder.')
    result['takeoff_landing_pass']=takeoff and landed and result['final_disarmed']
    return result


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('recording',type=Path)
    args=parser.parse_args()
    rows=[json.loads(line) for line in args.recording.read_text(encoding='utf-8-sig').splitlines() if line.strip()]
    print(json.dumps(analyze(rows),ensure_ascii=False,indent=2))
