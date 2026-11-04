# Assertion, libraries and renderer integration

The user's three-part goal is active: resolve the assertion collision, integrate
the engine dependencies and connect rendering to reach the existing game on
Switch. Full game boot is still pending.

## Assertion and library progress

Newlib puts `__assert` and `__assert_func` in one archive member. The Switch
runtime now supplies both entry points and routes both to the existing game
assertion report path. The real engine link has no duplicate definitions.
Desktop profiling definitions are removed from the Switch compile selection.

The pinned Luau bytecode compiler/VM and fastgltf/simdjson now build and link
as ARM64 static libraries. Switch Luau uses the interpreter; its executable
memory/JIT backend remains disabled explicitly. A generated simdjson header
overlay omits only its optional mmap helper, retaining its ordinary buffered
file and byte parsing APIs. The pinned VM also receives its missing standard
`climits` include. Acquired PC dependency source trees remain unchanged.

The complete build now compiles 427 ARM64 objects and links the engine with
zero unresolved symbols or duplicate definitions. SDL3, media, data/encoding
and rendering dependencies are included. The PC game/tests rebuild; all 60
selected archive, datapack, Luau and crash regression cases pass.

## Mesa/NVK route

[Mesa](https://mesa3d.org/) supplies the NVK NVIDIA Vulkan driver. The
[NXVK project](https://github.com/PalindromicBreadLoaf/nxvk) adapts it to
Horizon/libnx, including native-window presentation. This work uses the
experimental packaged Mesa 25.0.7 driver from
[nvk-switch](https://github.com/jhuz0/nvk-switch/releases/tag/nvk-switch-mesa-25.0.7-r1),
with exact package/library hashes checked. The SDK's deko3d remains a fallback
if workload testing exposes a driver limitation.

The 0.5.0 capability probe compiles the unchanged Kazarov requirements source.
Album capture `2026110418110300-5361907E67981C0712C80F39FBC192A1.jpg`
(console timestamp 2026-11-04 18:11:03) and its saved result confirm:

- NVIDIA Tegra X1 (`NVK gm20b`), Vulkan API 1.3.
- All engine **offscreen capability requirements** pass, including its real
  descriptor, blending and depth/stencil limits.
- A GPU command fills 1,024 words and CPU readback verifies every word.
- Failure bits `0x00`.

This establishes advertised capabilities and a real command/readback round trip.
It does not establish shader/draw/swapchain correctness or game performance.
Evidence is under `port/build/switch/vulkan-probe/hardware`.

## Real renderer probe 0.6.0

The real engine renderer is linked without rewriting its gameplay-facing API.
`prepare_vulkan_dispatch.py` generates typed dispatch for the 105 Vulkan calls
the engine needs, using its pinned Khronos prototypes. It routes them through
the static ICD, tracking the single renderer instance/device; missing commands
fail through the existing fatal hook. The Kazarov submodule is unchanged.

SDL3 3.4.18 uses the pinned public homebrew backend plus a small owned extension
that creates `VK_NN_vi_surface` over libnx's default `NWindow`. SDL exit events
use its native applet event pump. Unsupported POSIX `sigaction` is disabled;
file flushing uses the SDK's real `fsync` implementation rather than fdatasync.

Probe 0.6.0 creates the actual renderer and prog0 pipelines, presents through
its swapchain, and checks GPU pixels for clear/untextured drawing, RGBA upload,
indexed/palette drawing and GS blending. Its PASS/FAIL display is drawn by the
engine renderer itself. It writes logs, `result.txt` and an initial readback
`frame.png` under `sd:/switch/ChronicleTwo-PC-Switch/renderer-probe`.

The uploaded NRO is 19,425,477 bytes, SHA-256
`cb36bb6ea1920e03957c2e255eb59b875b9f96f944b2784d6f65d0c5cf232dfe`.
NRO and attribution files were downloaded again and checked by size and hash.
Hardware execution is pending. Close the previous probe fully before launching
**ChronicleTwo real renderer probe**. Initial pipeline compilation may leave a
blank screen; inspect its stdout/stderr logs if it does not reach the display.

## Reproduce

The prepared PC tree supplies the pinned compiler/dependency and shader inputs.
Use native Windows CMake/Python and the SDK at `C:/devkitPro`:

```powershell
python scripts/switch/fetch_dependencies.py
python scripts/switch/prepare_sdl.py
cmake -S port/switch/dependencies -B port/build/switch-dependencies-native -G Ninja -DCMAKE_TOOLCHAIN_FILE="$PWD/port/switch/toolchain-windows.cmake"
cmake --build port/build/switch-dependencies-native --target switch_libraries -j4
cmake -S port/switch/sdl -B port/build/switch-sdl3-vulkan -G Ninja -DCMAKE_TOOLCHAIN_FILE="$PWD/port/switch/toolchain-windows.cmake"
cmake --build port/build/switch-sdl3-vulkan --target SDL3-static -j4
python scripts/switch/audit_compile.py --services --objects --output port/build/switch-engine/objects
python scripts/switch/link_engine.py --reuse-objects --libraries port/build/switch-dependencies-native/libraries.json
python scripts/switch/build_vulkan_probe.py
python scripts/switch/build_renderer_probe.py
```

Set `CMAKE_MAKE_PROGRAM` to the acquired native Ninja explicitly if MSYS
executables precede it on PATH. External packages and outputs stay ignored.
Attribution files accompany the uploaded probes. `fetch_dependencies.py`
records pinned URLs/checksums and extracts only inside the workspace.

FFmpeg 7.1-5 and dav1d 1.5.0-1 official Switch packages are acquired. The movie
player compiles against the matching 7.1 headers rather than the PC build's
9.x headers. `assemble_libraries.py` prepares the combined link manifest;
the engine linker checks each archive's actual AArch64 machine type.

## Native data services

The Switch archive mount uses a native read-only ZIP64/Zstandard backend with
the same entry names, sizes, CRCs and byte payloads. It validates central/local
headers, ZIP64 bounds, UTF-8 paths, encryption flags and duplicate names, and
checks the CRC after decoding every entry. Buffered reads replace PC memory
mapping; decompression occurs outside the shared file lock. The PC continues
to use its original Rust backend and extraction/packing tooling.

Sixteen compatibility/corruption scenarios pass on the host. The native reader
also decodes and CRC-verifies all **5,775 entries** in the PC built-in asset
archive, with metadata independently matched against Python's ZIP reader.

The Switch uses the existing stb PNG fallback and requires prepared PC game
data instead of linking the desktop extractor GUI. Its CP932 backend uses
committed tables generated from the Windows reference APIs with strict error
and substitution handling. **79,112 cases** match the Windows baseline,
including all BMP scalars, single bytes, lead/trail combinations and malformed
UTF-8. It does not depend on a host iconv library. Windows API generation is
only needed to regenerate the tables; normal cross builds consume committed
data. Host checks are in `scripts/switch/data_tests`.

A complete **427/427 ARM64 object build and full engine link now pass**, with
zero unresolved symbols and zero duplicate definitions. All third-party link
inputs were checked as AArch64 objects. The PC reference rebuilds and all 60
selected archive, datapack, Luau and crash regressions pass.

## Native game preview 0.7.0

`build_game.py` now orchestrates the pinned dependency builds, native engine
compile/link and NRO packaging. The resulting `chronicletwo-switch.nro` is
44,861,637 bytes. It contains the real game `MainLoop`, renderer, native
platform adapters and data/media/script libraries. Hardware game boot is not
yet established. The default run root is the dedicated
`sdmc:/switch/ChronicleTwo-PC-Switch/game`; explicit launch arguments can
override it through the existing parser. First boot uses the tested single
graphics-thread mode and saves startup stdout/stderr there.

The complete orchestration also passes with `--reuse-objects`: dependencies
are rebuilt as needed, matching engine objects are verified, and the engine
is relinked and packaged. SDL preparation repairs older repeated insertions
and leaves source hashes unchanged on subsequent runs.

The original PC verifier passes the complete game archive, including 15,711
PNG textures. Its 91,149-entry archive is 3,809,445,609 bytes. The marker's
`output_sha256` is a digest of uncompressed entry contents, not a raw-file
checksum. The separately computed raw-file SHA-256 for transfer verification
is `b1133ba1c80f278b8236ba09b888c08757d97debf9fa84c90d038e0f3a28e2f4`.
The archive and its existing metadata are transferring into the dedicated
game folder. Only after complete byte/hash readback verification will the
game NRO be published for testing. Existing saves and the prototype remain
outside this staging workflow. Retail data and generated outputs stay out of Git.

Reproduce the complete native build with
`python scripts/switch/build_game.py --jobs 4`, or the `switch_game` host CMake
target after preparing the PC tree. A 0.6.0 renderer hardware result and a
0.7.0 game boot still remain to be verified. The goal remains active.
