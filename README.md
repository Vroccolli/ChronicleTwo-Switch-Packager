# ChronicleTwo Switch Packager

An offline Windows app that turns your supported **European PAL Dark Chronicle
ISO** into a ready-to-copy Switch SD-card folder. Users do not need Python,
compilers, SDKs, an internet connection, or a PC game installation.

This packager installs the native ChronicleTwo Switch engine, built from recovered
game source and the original MainLoop. It does not translate the PS2 executable
from the ISO. The ISO provides the original game assets.

## Use

1. Download `ChronicleTwo-Switch-Packager.zip` from this repository's Releases.
2. Extract the entire ZIP. Keep `_internal` beside the executable.
3. Open `ChronicleTwo-Switch-Packager.exe`.
4. Select your PAL ISO and an output folder with at least **12 GiB free**.
5. Choose **Create Switch folder** and keep the window open until it finishes.
6. Copy the resulting `switch` folder to your SD-card root and launch ChronicleTwo
   in full-application homebrew mode.

Conversion can take 15 minutes or longer and uses several GiB of memory. The ISO
is read only. US Dark Cloud 2 and modified disc images are not supported.

The supported raw ISO SHA-256 is:

```text
3073ac3773784e580801e833e0c9e1d96023af46a01d76bcb698a58e1bbd61a6
```

An existing installation is never overwritten. Choose a fresh output folder;
back up saves before manually updating an SD card. Failed runs leave an explicitly
unfinished folder with `packager.log`, and never publish a completed installation.

## What is verified

The packager checks its bundled runtime hashes, validates the ISO, converts the
disc through the PC project's `dc2data` pipeline, runs full archive verification,
then writes `archive.sha256` and `installation.json`. All work is staged on the
output volume before publication.

The included native engine is a **preview** with ongoing performance and
compatibility work. Binary/build verification is distinct from hardware testing
or campaign completion. The installation receipt records the exact version and
hardware-test status. Current package: engine **0.7.13**, not hardware-tested.

No retail ISO or completed retail game-data archive is included in this source
repository or the packager download. The bundled gameplay seed is generated from
the engine's recovered source; retail assets are extracted from your own ISO.

## Development

The GUI and workflow are in `scripts/packager/packager.py`. Python 3.11+ with Tk is
required for development only. Run the focused workflow tests with:

```powershell
python -m unittest discover -s scripts/packager -p test_packager.py -v
```

To build the Windows package, first prepare the separate native Switch engine
checkout, including its full engine NRO/report, staged dependency notices and
Windows `dc2data.exe`, DLLs and matching `assets/data.zip` gameplay seed. This
repository contains the packager; it does not build the engine or extractor.
The upstream PC/decompilation project alone does not contain these Switch builds.

```powershell
python -m venv .venv
.venv/Scripts/python.exe -m pip install -r scripts/packager/requirements-build.txt
.venv/Scripts/python.exe scripts/packager/build_release.py --engine-root ../ChronicleTwo-PC-Switch
```

The developer builder checks the complete-engine report and NRO hash, resolves
extractor DLL dependencies, includes the matching gameplay seed and notices,
and creates the standalone app and ZIP under `dist`. A normal user only needs
the published ZIP.

## Validation

On 2026-11-05 the frozen Windows EXE completed a fresh PAL-ISO installation with
exit code 0. All 307,847 publication verification entries passed, followed by a
separate check of all 15,711 PNG textures. The generated archive has 91,149 entries
and its uncompressed-content digest matches the prepared PC baseline. Eight
focused workflow tests and the distributable ZIP's CRC check pass.

## Credits

Based on the [ChronicleTwo](https://github.com/TheMoonPeople/ChronicleTwo) native
PC/decompilation project and its Kazarov extraction tooling. Bundled components
retain their respective notices and licenses. This is an unofficial fan project
and is not affiliated with Sony or Level-5.
