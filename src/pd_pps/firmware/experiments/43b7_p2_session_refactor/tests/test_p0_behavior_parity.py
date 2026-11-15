"""Compile baseline and refactor together, run deterministic regression on host."""
import hashlib
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]
BASE=ROOT/'tests/fixtures'
POLICY_SHA='a0700b80d38c1afa6dec56ebc30a91714bba218e4e4691a9e8af597c1a7e9be3'
USB_SHA='dc6ef5be19dcd984ec1b3f91993fa6a17c3c81f9a2ef086fb9ecdf84ca53bdd1'

class P0BehaviorParity(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.cc=shutil.which('gcc') or shutil.which('clang')

    def require_compiler(self):
        if not self.cc:
            self.skipTest('Native compiler unavailable; P0 parity must be run on build host')

    def compile_and_run(self, paths, cflags=()):
        self.require_compiler()
        with tempfile.TemporaryDirectory(prefix='m43b6-p0-') as tmp:
            exe=Path(tmp)/('test.exe' if sys.platform=='win32' else 'test')
            cmd=[self.cc,'-std=c11','-O2','-Wall','-Wextra','-Werror',
                 '-I'+str(ROOT/'tests/stubs'),'-I'+str(ROOT/'src'),*cflags,
                 *map(str,paths),'-o',str(exe)]
            built=subprocess.run(cmd,capture_output=True,text=True)
            self.assertEqual(built.returncode,0,built.stderr)
            ran=subprocess.run([str(exe)],capture_output=True,text=True)
            self.assertEqual(ran.returncode,0,ran.stdout+ran.stderr)
            return ran.stdout

    def test_original_fixtures_are_pinned(self):
        self.assertEqual(hashlib.sha256((BASE/'pd_policy_m43b5_v01.c').read_bytes()).hexdigest(),POLICY_SHA)
        self.assertEqual(hashlib.sha256((BASE/'m38_usb_m43b5_v01.c').read_bytes()).hexdigest(),USB_SHA)

    def test_policy_event_and_tick_parity(self):
        # Rename only baseline public symbols, then compile all TUs together.
        self.require_compiler()
        symbols=['pe_init','pe_event','pe_tick','pe_command_result',
                 'pe_request_soft_reset','pe_request_source_caps']
        macros=[f'-D{s}=baseline_{s}' for s in symbols]
        with tempfile.TemporaryDirectory(prefix='m43b6-policy-') as tmp:
            original=Path(tmp)/'original.o'; refactored=Path(tmp)/'refactored.o'
            executable=Path(tmp)/('compare.exe' if sys.platform=='win32' else 'compare')
            def run(cmd):
                result=subprocess.run(cmd,capture_output=True,text=True)
                self.assertEqual(result.returncode,0,repr(cmd)+'\n'+result.stdout+'\n'+result.stderr)
                return result
            common=[self.cc,'-std=c11','-O2','-Wall','-Wextra','-Werror','-I'+str(ROOT/'src')]
            run([*common,*macros,'-c',str(BASE/'pd_policy_m43b5_v01.c'),'-o',str(original)])
            run([*common,'-c',str(ROOT/'src/pd_policy.c'),'-o',str(refactored)])
            run([*common,str(original),str(refactored),str(ROOT/'tests/test_policy_parity_runtime.c'),'-o',str(executable)])
            result=run([str(executable)])
            self.assertIn('POLICY_PARITY_PASS',result.stdout)

    def test_cdc_wire_output_parity(self):
        common=[ROOT/'src/m42_diag.c',ROOT/'src/pd_vbus_history.c',
                ROOT/'tests/test_cdc_index_parity_runtime.c']
        baseline=self.compile_and_run([BASE/'m38_usb_m43b5_v01.c',*common])
        refactor=self.compile_and_run([ROOT/'src/m38_usb.c',*common])
        self.assertEqual(refactor,baseline)
        self.assertIn('ERR PDTRACE index must be 0..126',refactor)
        self.assertIn('ERR VBLOG index must be 0..39',refactor)

if __name__=='__main__':unittest.main()
