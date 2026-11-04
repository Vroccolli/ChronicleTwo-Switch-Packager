"""Run actual terminating children and inspect persisted reports and exit status."""
from pathlib import Path
import subprocess
import sys

exe, root = sys.argv[1], Path(sys.argv[2]).resolve()
for mode, status, marker in [("write", 0, "<save>/report marker"),
                             ("fatal", 17, "controlled marker"),
                             ("terminate", 1, "std::terminate called"),
                             ("fatal-hook", 1, "fatal hook marker"),
                             ("worker", 23, "worker marker"),
                             ("no-root", 17, "controlled marker")]:
    folder = root / mode
    folder.mkdir(parents=True, exist_ok=True)
    report = folder / "crashes/switch-last-report.txt"
    report.unlink(missing_ok=True)
    result = subprocess.run([exe, mode, str(folder)], capture_output=True, timeout=5)
    assert result.returncode == status, (mode, result.returncode, result.stderr)
    text = report.read_text() if mode != "no-root" else result.stderr.decode(errors="replace")
    for wanted in [marker, "Dark Chronicle crash report", "services-test", "frame: 77", "module base:", "frame 0:", "module+0x"]:
        assert wanted in text, (mode, wanted, text)
    if mode != "no-root":
        assert str(folder) not in text, (mode, text)
        assert "last asset: services-test/asset.bin" in text
    if mode == "worker":
        assert " worker\n" in text
    print(mode + ": PASS")
