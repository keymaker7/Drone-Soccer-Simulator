"""Internal analyzer fixtures only; no joystick or real recording is written."""
import importlib.util
import pathlib
import unittest

spec=importlib.util.spec_from_file_location('flight_analysis',pathlib.Path(__file__).with_name('analyze-flight-session.py'))
module=importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


def fixture():
    result=[]
    for i in range(100):
        z=.2 if i<10 or i>70 else 1.
        result.append(dict(selected=0,source_sha256='internal-math-fixture',wall_s=i*.05,
                           radio_connected=True,radio_enabled=True,aircraft=[dict(id=0,position_m=[0,0,z],speed_mps=0,
                           throttle=.0 if z<.3 else .5,tilt_deg=0,armed=i<90,input_source='USB',autonomous=False,flight_mode='ANGLE')]))
    return result


class Tests(unittest.TestCase):
    def test_takeoff_hover_landing(self):
        result=module.analyze(fixture())
        self.assertTrue(result['takeoff_landing_pass'])
        self.assertTrue(result['usb_hover_observed'])

    def test_no_takeoff_from_idle_or_fixture_or_autonomy(self):
        for condition in ('idle','fixture','auto'):
            rows=fixture()
            for r in rows:
                if condition=='idle':r['aircraft'][0]['position_m'][2]=.2
                if condition=='fixture':r['aircraft'][0]['input_source']='VERIFICATION_FIXTURE'
                if condition=='auto':r['aircraft'][0]['autonomous']=True
            self.assertFalse(module.analyze(rows)['takeoff_landing_pass'])

    def test_loss_in_air_is_not_controlled_landing(self):
        rows=fixture()
        for r in rows[45:]:r['radio_enabled']=False
        result=module.analyze(rows)
        self.assertTrue(result['usb_takeoff_observed'])
        self.assertFalse(result['uninterrupted_usb_landing_observed'])

    def test_long_gap_breaks_continuity(self):
        rows=fixture()
        for r in rows[45:]:r['wall_s']+=1
        self.assertFalse(module.analyze(rows)['uninterrupted_usb_landing_observed'])

    def test_invalid_time_and_mixed_build_rejected(self):
        for change in ('time','build','nan'):
            rows=fixture()
            if change=='time':rows[30]['wall_s']=-1
            if change=='build':rows[30]['source_sha256']='other'
            if change=='nan':rows[30]['aircraft'][0]['position_m'][2]=float('nan')
            with self.assertRaises(ValueError):module.analyze(rows)


if __name__=='__main__':unittest.main(verbosity=2)
