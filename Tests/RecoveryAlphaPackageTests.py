"""Exercise release gates and reject accidental test/profile leakage into the ZIP."""
import argparse
import importlib.util
import json
import tempfile
import unittest
import zipfile
from pathlib import Path
from types import SimpleNamespace

parser = argparse.ArgumentParser()
parser.add_argument('--repo', type=Path, required=True)
args, rest = parser.parse_known_args()
spec = importlib.util.spec_from_file_location('packager', args.repo / 'Tools/Package-RecoveryAlpha.py')
package = importlib.util.module_from_spec(spec)
spec.loader.exec_module(package)

class RecoveryPackageTests(unittest.TestCase):
    def fixture(self, base):
        root = base / 'stage'
        root.mkdir()
        (root / 'VERSION.txt').write_text('test')
        manifest = {'source_commit': 'a' * 40, 'files': {'VERSION.txt': package.sha(root / 'VERSION.txt')}}
        (root / 'RELEASE_MANIFEST.json').write_text(json.dumps(manifest))
        acceptance = {'source_commit': 'a' * 40, 'github': {'head_sha': 'a' * 40,
            'checks': [{'name': name, 'conclusion': 'SUCCESS'} for name in package.REQUIRED_CI]},
            'local': {'ctest_exit_code': 0},
            'packaged_smoke': {name: {'passed': True} for name in package.REQUIRED_SMOKE}}
        receipt = base / 'validation.json'
        receipt.write_text(json.dumps(acceptance))
        return root, receipt, acceptance

    def test_archive_excludes_smoke_profile_and_checks_crc(self):
        with tempfile.TemporaryDirectory() as directory:
            base = Path(directory)
            root, receipt, _ = self.fixture(base)
            (root / 'test-profile-secret.txt').write_text('must not ship')
            output = base / 'alpha.zip'
            package.archive(SimpleNamespace(stage=root, validation=receipt, output=output))
            with zipfile.ZipFile(output) as archive:
                self.assertNotIn('test-profile-secret.txt', archive.namelist())
                self.assertIsNone(archive.testzip())
                self.assertIn('VALIDATION.json', archive.namelist())
            self.assertIn(package.sha(output), output.with_suffix('.zip.sha256').read_text())

    def test_rejects_wrong_revision_failed_ci_or_missing_gui_proof(self):
        for mutation in ('revision', 'ci', 'gui'):
            with self.subTest(mutation=mutation), tempfile.TemporaryDirectory() as directory:
                base = Path(directory)
                root, receipt, acceptance = self.fixture(base)
                if mutation == 'revision': acceptance['github']['head_sha'] = 'b' * 40
                elif mutation == 'ci': acceptance['github']['checks'][0]['conclusion'] = 'FAILURE'
                else: acceptance['packaged_smoke']['river']['passed'] = False
                receipt.write_text(json.dumps(acceptance))
                output = base / 'alpha.zip'
                with self.assertRaises(ValueError):
                    package.archive(SimpleNamespace(stage=root, validation=receipt, output=output))
                self.assertFalse(output.exists())

    def test_rejects_changed_staged_file_and_recursive_zip_output(self):
        with tempfile.TemporaryDirectory() as directory:
            base = Path(directory)
            root, receipt, _ = self.fixture(base)
            with self.assertRaises(ValueError):
                package.archive(SimpleNamespace(stage=root, validation=receipt, output=root / 'self.zip'))
            (root / 'VERSION.txt').write_text('changed after testing')
            with self.assertRaises(ValueError): package.verify(root)

    def test_stale_native_build_cannot_be_staged_as_a_new_commit(self):
        with tempfile.TemporaryDirectory() as directory:
            binary = Path(directory) / "native.exe"
            binary.write_bytes(b"fixture" + b"a" * 40)
            package.require_native_revision(binary, "a" * 40)
            with self.assertRaises(ValueError): package.require_native_revision(binary, "b" * 40)

    def test_rejects_unsafe_previous_archive_paths(self):
        for name in ('../outside', '/absolute', 'C:/absolute', 'dir\\file', 'safe/../../outside'):
            with self.subTest(name=name), self.assertRaises(ValueError): package.safe_relative(name)

unittest.main(argv=['RecoveryAlphaPackageTests', *rest])
