"""Bundled-source validation fixtures. Does not generate pilot or aircraft telemetry."""
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'Bridge'))
from sdacs_bridge import GuidanceBridge


class BundledSourceTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory()
        self.root=Path(self.temp.name)/'upstream'
        repo=Path(os.environ['SDACS_REPO'])
        files=['simulation/apf_engine/apf.py','LICENSE']
        hashes={}
        for relative in files:
            p=self.root/relative;p.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(repo/relative,p)
            hashes[relative]=hashlib.sha256(p.read_bytes()).hexdigest()
        self.meta=dict(origin='https://github.com/sun475300-sudo/swarm-drone-atc.git',commit=subprocess.check_output(['git','-C',str(repo),'rev-parse','HEAD'],text=True).strip(),files=hashes)
        self.save()
    def tearDown(self):self.temp.cleanup()
    def save(self): (self.root/'provenance.json').write_text(json.dumps(self.meta),encoding='utf-8')
    def test_no_git_needed(self):
        with patch('sdacs_bridge._git',side_effect=AssertionError('Git must not run')):
            bridge=GuidanceBridge(self.root,bundled=True)
        self.assertEqual(bridge.health()['source_commit'],self.meta['commit'])
        self.assertEqual(bridge.health()['apf_sha256'],self.meta['files']['simulation/apf_engine/apf.py'])
    def test_tampered_apf_rejected(self):
        with (self.root/'simulation/apf_engine/apf.py').open('a',encoding='utf-8') as f:f.write('\n# damaged distribution\n')
        with self.assertRaises(RuntimeError):GuidanceBridge(self.root,bundled=True)
    def test_missing_license_rejected(self):
        del self.meta['files']['LICENSE'];self.save()
        with self.assertRaises(RuntimeError):GuidanceBridge(self.root,bundled=True)
    def test_path_escape_rejected(self):
        outside=self.root.parent/'outside';outside.write_bytes(b'fixture')
        self.meta['files']['../outside']=hashlib.sha256(b'fixture').hexdigest();self.save()
        with self.assertRaises(RuntimeError):GuidanceBridge(self.root,bundled=True)
    def test_invalid_origin_commit_rejected(self):
        for key,value in [('origin','https://example.org/other.git'),('commit','x'*40)]:
            previous=self.meta[key];self.meta[key]=value;self.save()
            with self.assertRaises(RuntimeError):GuidanceBridge(self.root,bundled=True)
            self.meta[key]=previous

if __name__=='__main__':unittest.main(verbosity=2)
