from pathlib import Path
import io
import json
import tempfile
import unittest
from unittest.mock import patch

from packager import APP, digest, install, run_tool


class InstallationTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.iso = self.root / "Game with spaces.iso"
        self.iso.write_bytes(b"fixture disc")
        self.payload = self.root / "payload"
        for name in ("tools/dc2data.exe", "tools/assets/data.zip", "runtime/chronicletwo-switch.nro", "runtime/glyphs/glyphs.json"):
            p = self.payload / name
            p.parent.mkdir(parents=True, exist_ok=True)
            p.write_bytes(b"fixture runtime")
        self.manifest = {"switch_version": "test", "hardware_tested": False,
                         "supported_iso_sha256": [digest(self.iso)], "minimum_free_bytes": 1,
                         "files": {p.relative_to(self.payload).as_posix(): {"bytes": p.stat().st_size, "sha256": digest(p)}
                                   for p in self.payload.rglob("*") if p.is_file()}}
        self.write_manifest()
        self.output = self.root / "Output with spaces"
        self.calls = []

    def write_manifest(self):
        (self.payload / "manifest.json").write_text(json.dumps(self.manifest))

    def runner(self, command, log, report):
        self.calls.append(command)
        if command[1] == "extract":
            game = Path(command[3]).parent
            (game / "data.zip").write_bytes(b"verified archive")
            (game / "data.json").write_text("{}")
        log.write("fixture checked\n")

    def install(self, runner=None):
        return install(self.iso, self.output, self.payload, lambda s: None, runner or self.runner)

    def test_complete_installation_and_receipt(self):
        target = self.install()
        self.assertEqual(target, self.output / "switch" / APP)
        self.assertEqual([c[1] for c in self.calls], ["extract", "check"])
        self.assertEqual((target / "game/archive.sha256").read_text(),
                         f"16 {digest(target / 'game/data.zip')}\n")
        receipt = json.loads((target / "installation.json").read_text())
        self.assertTrue(receipt["archive_verified"])
        self.assertFalse(receipt["hardware_tested"])
        self.assertEqual(receipt["iso_sha256"], digest(self.iso))
        self.assertFalse(list(self.output.glob("ChronicleTwo-unfinished-*")))

    def test_existing_installation_preserved(self):
        target = self.install()
        sentinel = target / "saves/keep.dat"
        sentinel.parent.mkdir()
        sentinel.write_bytes(b"save")
        with self.assertRaisesRegex(ValueError, "already exists"):
            self.install()
        self.assertEqual(sentinel.read_bytes(), b"save")

    def test_unknown_disc_creates_no_output(self):
        self.iso.write_bytes(b"unsupported")
        with self.assertRaisesRegex(ValueError, "Unsupported disc"):
            self.install()
        self.assertFalse(self.output.exists())

    def test_corrupt_bundle_refused_before_output(self):
        (self.payload / "tools/dc2data.exe").write_bytes(b"tampered")
        with self.assertRaisesRegex(ValueError, "damaged"):
            self.install()
        self.assertFalse(self.output.exists())

    def test_failed_verification_never_publishes(self):
        def fail(command, log, report):
            self.runner(command, log, report)
            if command[1] == "check":
                raise RuntimeError("CRC mismatch")
        with self.assertRaisesRegex(RuntimeError, "CRC mismatch"):
            self.install(fail)
        self.assertFalse((self.output / "switch" / APP).exists())
        self.assertTrue(list(self.output.glob("ChronicleTwo-unfinished-*/packager.log")))

    def test_low_disk_space_refused(self):
        with patch("packager.shutil.disk_usage") as usage:
            usage.return_value.free = 0
            with self.assertRaisesRegex(ValueError, "free space"):
                self.install()
        self.assertFalse(self.calls)

    def test_missing_archive_never_publishes(self):
        with self.assertRaisesRegex(RuntimeError, "did not produce"):
            self.install(lambda *args: None)
        self.assertFalse((self.output / "switch" / APP).exists())

    def test_subprocess_failure_is_propagated(self):
        import sys
        with self.assertRaisesRegex(RuntimeError, "preparation failed"):
            run_tool([sys.executable, "-c", "print('extractor error'); raise SystemExit(7)"], io.StringIO(), lambda s: None)


if __name__ == "__main__":
    unittest.main()
