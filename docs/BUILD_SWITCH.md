# Build the complete native Switch project

The repository has two branches: `main` is the standalone offline ISO packager;
`switch-engine` contains the full native engine plus the packager. Use this branch
to rebuild the game. Do not download the GitHub source ZIP: it omits the pinned
Kazarov submodule.

## Requirements

The supported development host is native Windows x64. Linux/macOS engine cross
builds are not established by this workflow. Install:

- Git for Windows with HTTPS access to GitHub.
- Python 3.11 or newer, including Tk, pip and venv.
- PowerShell 7 (`pwsh`) and 7-Zip (`7z`).
- Native Windows CMake 3.29 or newer; an MSYS CMake is not interchangeable.
- The devkitPro Switch SDK: devkitA64, libnx, switch-tools, switch-mesa,
  switch-libdrm_nouveau, switch-zlib, switch-libzstd and switch-bzip2.

Use the [official devkitPro installation instructions](https://devkitpro.org/wiki/Getting_Started)
and its package manager for the SDK. These are developer tools, not requirements
for users of the prebuilt packager. The locally tested SDK baseline is devkitA64
r30, libnx 4.12.0, switch-tools 1.13.1 and switch-mesa 20.1.0. The build checks for
the required compiler, tools, headers and libraries before compiling.

Expect a large first download and several GiB of RAM and disk use. Two build jobs
are the default to limit concurrent load. Allow additional space for the SDK,
compiler/dependency caches and both PC and ARM64 build outputs. Retail conversion
later requires at least 12 GiB free in its selected output folder.

## Fresh checkout

```powershell
git clone --branch switch-engine --recurse-submodules https://github.com/Vroccolli/ChronicleTwo-Switch-Packager.git
cd ChronicleTwo-Switch-Packager
python scripts/switch/build_from_source.py --check
python scripts/switch/build_from_source.py --jobs 2 --packager
```

For an existing clone, switch to `switch-engine`, pull its changes, and run
`git submodule update --init --recursive`. Keep the recorded submodule revision;
do not update Kazarov to its latest main branch. This checkpoint pins Kazarov to
`14164f13fb4fcd4395009b81ba3f3208df1e52b3`.

The build performs these stages, failing if any stage fails:

1. Acquire the pinned native Windows LLVM/Rust/Ninja/SDL/FFmpeg/shader dependencies
   using `tools/windows` and verify their downloaded archive hashes.
2. Configure/build the native PC target and `dc2data` extractor. This generates
   the source copies, script/gameplay seed and compilation database needed for
   ARM64; it does not extract a retail disc or start the PC game.
3. Fetch the pinned Switch SDL/backend, NVK, FFmpeg and dav1d archives. Build the
   Switch libraries, apply the tracked platform adapters to generated source
   copies and compile every shared engine unit for ARM64 with release flags.
4. Link the full engine, rejecting incomplete links, missing symbols, stale
   inputs and non-ARM64 libraries. Package the ELF as an NRO with version metadata.
5. Stage dependency notices and controller artwork from the matching sources.
6. With `--packager`, create a local Python environment, install the pinned
   freezing tools, and generate the offline Windows app and ZIP.

Omit `--packager` if you only need the NRO and extractor. No prebuilt game NRO,
old compilation database, local generated files or retail archive is required.
The system SDK and some acquired third-party components (notably the NVK driver
and FFmpeg) are distributed as prebuilt dependencies. This is a full game-source
build, not a claim to rebuild every external compiler and SDK library from source.
Their pinned source/package URLs and hashes are recorded in the scripts/notices.

## Tool paths

The default SDK root is the native `DEVKITPRO` value or `C:/devkitPro`.
The common MSYS virtual value `/opt/devkitpro` is mapped to the standard Windows
installation when present. For a different installation:

```powershell
python scripts/switch/build_from_source.py --devkitpro D:/devkitPro --jobs 2 --packager
```

Set `CT_CMAKE`, `CT_GIT`, `CT_PWSH` or `CT_7ZIP` to full executable paths if they
cannot be found. The runner passes its native Python through `CT_PYTHON` so the
PC scripts do not accidentally select an MSYS or different Python. Paths may
contain spaces. These settings are local; do not commit CMake caches or SDK files.

## Results and verification

- `port/build/switch/game/chronicletwo-switch.nro`: native game executable.
- `port/build/switch/game/build.json`: version, complete-engine counts and hashes.
- `port/build/switch-engine/link-report.json`: source/object/library link evidence.
- `port/build/windows-x64-release/dc2data.exe`: native Windows extractor.
- `port/build/packager-release/ChronicleTwo-Switch-Packager.zip`: offline app.

Extract the complete app ZIP, select your verified European PAL ISO, then choose
an output folder. The app generates `switch/ChronicleTwo-PC-Switch` with the NRO,
converted data, artwork, config, notices and installation receipt. Copy `switch`
to the SD root. Existing installations and saves are not overwritten by the app.

Focused packager tests can run without an SDK or ISO:

```powershell
python -m unittest discover -s scripts/packager -p test_packager.py -v
```

The Switch-specific host checks are documented in `docs/SWITCH_RUNTIME.md`,
`docs/SWITCH_SERVICES.md`, `docs/SWITCH_INTEGRATION.md` and `docs/SWITCH_GAMEPLAY.md`.
Building successfully is not a substitute for testing title, gameplay, controls,
audio, saves/reloads and scene transitions on a console. Version 0.9.0's new FPS
overlay was built and transfer-verified; its console appearance remains pending.

## Troubleshooting

- Missing `tools/kazarov`: initialize the pinned submodule.
- `--check` reports missing SDK files: install the named SDK packages; do not copy
  desktop libraries into the Switch build or bypass the architecture checks.
- A checksum fails: retain the message and reacquire that specific dependency.
  Do not change a pinned checksum just to accept an unverified download.
- Missing generated PC files: run the top-level source runner rather than invoking
  `build_game.py` on an unprepared tree.
- `--reuse-objects` is an advanced incremental mode for the prepared engine build;
  the fresh-source runner deliberately does not use it.
- A download URL is unavailable: the build stops. Network access to upstream
  dependency hosts is required on the first build; caches are not committed.
