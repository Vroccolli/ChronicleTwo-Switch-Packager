# ChronicleTwo PC-to-Switch

Separate development repository for cross-compiling ChronicleTwo's native PC
engine for Nintendo Switch. The goal is to run the recovered game code and
existing native game loop on Switch, adapting the platform services and renderer.
This is a source port, not translation of a compiled PC executable.

## Starting point

- ChronicleTwo: `cb910edeea375887aab8e00085b93150be4e081e`.
- Kazarov: `14164f13fb4fcd4395009b81ba3f3208df1e52b3` (pinned submodule).
- Source and history copied from the locally available upstream PC baseline.
  These revisions have not been checked against the latest online upstream.
- The separate scene-by-scene Switch prototype is not part of this branch.

The [PC baseline](docs/PC_BASELINE_2026-11-04.md) builds and renders the title,
opening, town and first dungeon. Launch the prepared local build with
`./run-pc.ps1`. The [Switch work](docs/SWITCH_STATUS.md) has a compiler audit
and buildable core-math and [logging diagnostic NROs](docs/SWITCH_LOGGING.md).
The logging backend passes focused host checks and its 0.2.1 SD-card hardware test.
The [memory and cooperative thread adapters](docs/SWITCH_RUNTIME.md) pass 28 host
checks. Their 0.3.0 runtime diagnostic passes all bounded checks on Switch hardware.
The [GCC/services checkpoint](docs/SWITCH_SERVICES.md) compiles all 426 audited
units into ARM64 objects. Engine linking now exposes concrete library gaps.
Services probe 0.4.1 passes its bounded hardware retest, verified from its log/report.
The active [library/renderer integration](docs/SWITCH_INTEGRATION.md) clears the
assertion collision and builds native libraries. The Mesa/NVK capability probe
passes on Switch; real renderer probe 0.6.0 is uploaded for hardware validation.
A full Switch game build and full-game hardware execution are not yet established.

See [the Switch work plan](docs/PC_TO_SWITCH.md) for scope and milestones.
The original documentation follows.

# Chronicle Two

[![PAL Progress]](https://decomp.dev/TheMoonPeople/ChronicleTwo/pal)

[PAL Progress]: https://decomp.dev/TheMoonPeople/ChronicleTwo/pal.svg?mode=shield&label=PAL&measure=matched_code_percent
[progress_link]: https://decomp.dev/TheMoonPeople/ChronicleTwo/

[<img src="https://decomp.dev/TheMoonPeople/ChronicleTwo/pal.svg?w=512&h=256" width="512" height="256" alt="PAL decompilation progress">][progress_link]

Chronicle Two is a decompilation project (and eventual port) of Dark
Chronicle/Dark Cloud 2 for the PlayStation 2.

## Building and running

Clone with submodules: `git clone --recurse-submodules https://github.com/TheMoonPeople/ChronicleTwo.git`

Place the PAL disc image `Dark Chronicle (PAL).iso` in `rom/pal/`. The build
runs in a container that [Kazarov](tools/kazarov/README.md) builds on first use,
so Docker or Podman must be installed.

- `./build.sh` builds the PAL executable (`ps2-pal`) and checks it against retail.
  `./build.sh ps2-pal elf objdiff` builds only the named targets; `CLEAN=1 ./build.sh`
  rebuilds from scratch.
- `./run.sh` builds and runs the Release port; `./run.sh ps2-pal` builds, masters a disc image and boots it in PCSX2.
- `build.bat` and `run.bat` do the same on Windows.
- `tools/kazarov/scripts/kazarov shell` opens a shell in the build container, with the
  project at `/work/chronicletwo`.
- `port/flatpak.sh` packages the PC port as a Flatpak, `chronicletwo.flatpak`, which CI
  also builds; `docs/FLATPAK.md` covers building, installing and its first start.

## Diffing

Build with `./build.sh ps2-pal elf objdiff`, then open the project in the objdiff
GUI. It reads the `objdiff.json` that the build writes at the root.
