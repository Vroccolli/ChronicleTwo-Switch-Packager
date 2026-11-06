# Offline Windows ISO packager

The end-user ZIP contains `ChronicleTwo-Switch-Packager.exe` and its `_internal`
directory. Extract the complete ZIP, open the executable, select the verified
European PAL ISO and an output directory, then choose **Create Switch folder**.
No development tools, Python installation, network or PC installation are needed.
The resulting `switch` directory can be copied to the SD-card root. Launch in
full-application homebrew mode, matching the native engine's hardware workflow.

The packager uses the native engine build from `port/build/switch/game`, checks
its NRO against `build.json`, and bundles the prebuilt Windows `dc2data.exe` with
its transitive DLL dependencies and matching source-derived gameplay seed. It
does not distribute the retail ISO or completed retail data archive.

The ISO is checked against `rom/pal/checksum.sha256`. Extraction calls
`dc2data extract ISO STAGE/game/data --jobs 2`, then `dc2data check STAGE/game/data`.
Only a successful extraction and full archive check may publish the installation.
There is no route that silently reuses an existing PC asset archive.

The output contains the NRO, controller glyphs, `game/data.zip`, `game/data.json`,
`game/archive.sha256`, and a default 30 FPS cap. The bootstrap supplies the tested
single graphics-thread default. `installation.json` records the executable
version, input hash, archive verification, file sizes/hashes and the build's
hardware-test status. A complete build and matching hashes are distinct from
hardware verification or campaign completion.

At least 12 GiB free is required on the output volume. A temporary directory is
created there and renamed into `switch/ChronicleTwo-PC-Switch` after success.
An existing installation is refused, including a concurrent install. Saves and
other installations are never updated. Failed extraction/checks leave an explicitly
unfinished directory and log for diagnosis; the error names its location.

## Developer build

Use Windows x64 Python with Tk, PyInstaller 6.16.0 and pefile. For example:

```powershell
python -m venv port/build/packager-venv
port/build/packager-venv/Scripts/python.exe -m pip install -r scripts/packager/requirements-build.txt
port/build/packager-venv/Scripts/python.exe scripts/packager/build_release.py
```

The builder requires the prepared native engine and Windows extractor build.
It does not rebuild either or change the Switch installation. It checks engine
completeness, the NRO digest, glyph compatibility and gameplay-only seed layout.
Default outputs are under `port/build/packager-release`, ignored by Git.

Run the focused workflow tests:

```powershell
python -m unittest discover -s scripts/packager -p test_packager.py -v
```

For a real-disc end-to-end check, call `install` in `scripts/packager/packager.py`
with the frozen package's `_internal/payload` directory. This exercises the exact
bundled extractor, seed and dependencies. Allow 15 minutes or longer. Tests using
fixture executables do not establish real asset conversion or hardware gameplay.

## Validation on 2026-11-05

The frozen Windows EXE completed a fresh PAL-ISO installation with exit code 0,
using its bundled extractor, gameplay seed and DLLs. Two conversion workers were
used after an earlier PC crash interrupted a four-worker run; the crash's cause
has not been established. Interrupted outputs were retained separately and were
not accepted as completed installations.

The extractor completed all 307,847 publication verification entries and published
91,149 archive entries. The separate `dc2data check` then verified 15,711 PNG
textures. The generated archive's uncompressed-content digest, gameplay plan and
data version match the prepared PC baseline. Its compressed size is 3,809,441,483
bytes; compressed archive size alone is not used for baseline content comparison.

The final installation contains native preview 0.7.10, assets, controller artwork,
dependency notices, checksums, a verification receipt and log. The receipt still
records `hardware_tested: false`; this test does not verify Switch execution of
0.7.10. Eight focused workflow tests pass. The distributable ZIP's CRC check and
all 32 bundled payload file checks pass. A ZIP SHA-256 sidecar is beside the build.

Local results: `port/build/packager-final/ChronicleTwo-Switch-Packager.zip` and
`port/build/packager-e2e-restart/switch/ChronicleTwo-PC-Switch/installation.json`.
