"""Validate the actual authored cage asset: topology, dimensions and aperture bound."""
from collections import Counter
import json
import math
from pathlib import Path
import unittest

class CageTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):cls.asset=json.loads((Path(__file__).resolve().parents[1]/'Content/Data/cage-geodesic.json').read_text())
    def test_closed_spherical_topology(self):
        a=self.asset;self.assertEqual(len(a['nodes'])-len(a['edges'])+len(a['cells']),2)
        counts=Counter(tuple(sorted((p,q))) for cell in a['cells'] for p,q in zip(cell,cell[1:]+cell[:1]))
        self.assertEqual(set(counts.values()),{2});self.assertEqual(set(counts),{tuple(e) for e in a['edges']})
        self.assertEqual(Counter(map(len,a['cells'])),{5:12,6:30})
    def test_nominal_class40_envelope(self):
        a=self.asset
        for node in a['nodes']:
            self.assertTrue(all(math.isfinite(x) for x in node))
            self.assertAlmostEqual(math.sqrt(sum(x*x for x in node)),a['radius_cm'],places=5)
        self.assertAlmostEqual(2*a['radius_cm']+a['rod_diameter_cm'],40)
    def test_aperture_area_upper_bound(self):
        a=self.asset;areas=a['cell_opening_upper_bounds_cm2']
        self.assertEqual(len(areas),42);self.assertTrue(all(0<x<150 for x in areas))
        self.assertAlmostEqual(sum(areas),4*math.pi*20**2,places=3)
        self.assertAlmostEqual(max(areas),a['max_opening_upper_bound_cm2'],places=4)
    def test_balanced_cage_centroid(self):
        for axis in range(3):self.assertAlmostEqual(sum(p[axis] for p in self.asset['nodes']),0,places=5)

if __name__=='__main__':unittest.main(verbosity=2)
