# Native gameplay, persistence and performance

The real ChronicleTwo engine boots on Switch. On 2026-11-05 the user reached
the menus, started the intro and played the game in preview 0.7.1. The saved
logs contain real map, event, character and audio progression through the
original `MainLoop`. The Album capture
`2026110504110000-5361907E67981C0712C80F39FBC192A1.jpg` also shows the game's
save menu, rather than the separate scene prototype. First gameplay was slow:
the user reported about 11 FPS in the intro, and grainy audio.

## Preview 0.7.3

The initial cross-compile audit reused preprocessor flags but omitted release
optimization. `NDEBUG` alone did not optimize any of the 427 engine objects.
All are now rebuilt with `-O2 -mtune=cortex-a57`, retaining signed-char,
overflow/aliasing compatibility flags and `-ffp-contract=off`. Link reuse checks
both the profile metadata and each actual object command, preventing a silent
return to unoptimized engine objects. Extra platform C objects use `-O2` too.
The PC compiler/profile and pinned Kazarov source remain unchanged.

The 39,856,325-byte 0.7.3 NRO and a config with `video.max_fps=30.0` were
uploaded and fully size/SHA-256 readback verified. Only that config field was
changed; original remote configs were backed up locally. Receipt:
`port/build/switch/game/app-upload-verification-0.7.3.json`.

Normal startup no longer performs a whole-archive hash. `--verify-assets`
explicitly requests the libnx SHA-256 diagnostic; a missing checksum sidecar
then fails that requested check. Metadata/header validation, per-entry CRC
checks and script timing validation remain enabled. Prior complete host FTP
readback and on-device hash evidence for the installed archive are retained.

FPS/tick/draw counts are logged once per second. Switch mixer callback timing
is accumulated without allocation or logging on the audio callback; the game
thread reports counts, mean/peak microseconds and over-budget callbacks every
five seconds. These are callback timing budgets, not hardware underrun counts.

The user confirms 0.7.3 plays and saves. They report reaching 30 FPS with their
own overclock, while stock performance remains slower. No clock settings were
changed by this work. Saved metrics under
`port/build/switch/hardware-pulls/audio-baseline-20261105T100418Z` show many
26–30 FPS samples, some faster light scenes, callback means around 1–1.5 ms
and zero mixer timing-budget misses while grainy audio is still reported.
There is no controlled stock-clock before/after benchmark yet.

The configured maximum currently limits interpolated presentations, but
`PresentTickEnd()` and the non-interpolated branch of `BetweenTicks()` call
`PresentCanonicalFrame()` without consulting `PresentPacer::Next()`. Thus
the setting was not a strict 30 FPS ceiling: the captured light-scene samples
above 30 FPS agree with the 0.7.3 source. Preview 0.7.5 corrects this bypass
as described below.

## Switch save publication

The save failure was caused by desktop filesystem assumptions: directory
`fsync`, hard-link publication after exclusive rename returned `ENOSYS`, and
rename-over-existing are not provided by Horizon's SD devoptab in the same
way as desktop POSIX. The adapter in `port/switch/data/files.cpp` replaces
only the Switch build's shared file API. New files are flushed and published
with native exclusive rename. Replacements use a flushed journal, temporary
file and old-generation backup, with recovery before reads/scans. Unknown or
malformed recovery records fail closed and leave unrelated files intact.

Config writes use the same Switch publication path, and save/config reads
recover pending transactions first. The original PC implementation is kept.
The installed SDK's `lstat` devoptab entry was verified to use `fsdev_stat`.

Fifty-two isolated host checks pass, covering fresh writes, collisions,
overwrites, injected IO failures, interrupted rotations, rollback recovery,
malformed/traversal journals and racing first saves. The adapter and callers
also compile with the real ARM64 compiler. The user reports saving now works;
overwrite/reload/autosave hardware cases still need explicit confirmation.

The preserved PC game and tests rebuild successfully. The selected existing
save/config/audio/presentation regression run reports 117 passed and three
skipped retail-text fixture tests. These checks do not prove Switch performance
or power-loss behavior.

## Preview 0.7.4

A native initial loading screen shows the current startup stage while game
data and scripts are prepared. It uses the SDK console renderer directly so
stdout/stderr can continue writing to SD logs. The console framebuffer is
released before SDL/Vulkan takes the window. `game/startup-stage.txt` is closed
after each stage update, allowing FTP inspection while startup logs are locked.
Explicit checksum diagnostics show real byte progress on that screen too.

The remaining grainy audio is addressed in the Switch SDL AUDOUT backend.
The original backend queued one chunk and waited for that sole chunk to finish
before producing another, draining the hardware queue each cycle. Continuous
two-buffer playback now primes one silent packet before starting and reuses
only the buffer identified by AUDOUT's released tag. Tests call the actual
backend callbacks and pass 1,542,688 checks covering continuous PCM, queued
buffer ownership, release ordering, timeouts, errors and cleanup. The backend
also compiles with the production SDK and builds into the full engine.

The complete 427-object optimized engine links successfully for 0.7.4.
The 39,880,901-byte NRO was uploaded alone and verified by remote size and full
SHA-256 readback (`a4ed59b6335e67a21d2a2a218df0bb4ea44b1fda8e9b861da095cb892bf961d3`).
Receipt: `port/build/switch/game/app-upload-verification-0.7.4.json`.
Loading-screen appearance and audio improvement require a hardware retest.
An FTP read immediately after deployment still returned a stdout header of
0.7.3 and no `startup-stage.txt`; those old logs cannot verify 0.7.4. They were
retained under `port/build/switch/hardware-pulls/preview-0.7.4-20261105T101614Z`.

## Preview 0.7.5

The Switch synchronous canonical presentation path now consults the same
pacer as interpolated displays. When the next display is not due, it returns
without waiting, leaving simulation and canonical rendering active. This
covers both tick-end and non-interpolated presentations. Unbounded capture
and the PC presentation path retain their previous behavior. This limits
excess displays; it does not remove the cost of recording/rendering each
canonical game frame or establish 30 FPS in heavy scenes.

Eight presentation timing tests pass, including a deterministic 600-field
simulation with 300 displays at a 30 FPS cap, shared canonical/interpolated
deadlines, and no catch-up bursts after a stall. The PC game and test targets
rebuild successfully. The changed presentation source compiles with the real
ARM64 release command, and all 427 engine objects link successfully. The
compile audit now tracks the presentation timing header as a source dependency
for object reuse. Hardware frame pacing, startup appearance and audible audio
improvement remain unverified until the user runs the updated game.
The 39,880,901-byte NRO was deployed alone and fully size/SHA-256 readback
verified. SHA-256: `fa50fc42d21711931d485bef8aee2f93f3714672e29d25a3a5bef7d9e4d5f545`.
Receipt: `port/build/switch/game/app-upload-verification-0.7.5.json`.

### Camera-fade hardware regression

The user confirms the 0.7.5 loading screen appears and audio sounds better.
After a camera fade in the same room where saving had worked, performance
dropped persistently to about 5 FPS. Captured logs under
`port/build/switch/hardware-pulls/transition-0.7.5-20261105T110210Z` confirm the
0.7.5 build and startup completion after 84,594 ms. Before the drop, many
samples were 25–30 FPS; afterward they remained about 4.5–5.1 FPS for more
than 40 samples. Audio delivered 234–243 mixer callbacks per five-second
interval, with means around 1.2 ms and no callback-budget misses. This is
consistent with fixing the earlier audio queue gaps, but does not measure
AUDOUT underruns directly. The displayed tick rate counts wall-clock fields
and does not establish that logic frames continue at 60 Hz.

The cause of the sustained graphics slowdown is unproven. One candidate is
the display-list matcher's persistent `g_instanced_keys`: a transition may
route later keys through nested geometry comparisons until a full interpolation
cut. No matching behavior has been changed based on this hypothesis.

Preview 0.7.6 adds five-second timing summaries for completed end-frame calls,
canonical rendering, readback, display rendering, canonical presentation,
interpolation and mesh matching. Static Vulkan wrappers also time image
acquisition, fence waits, queue submission and queue presentation. These are
nested wall-clock intervals, not additive exclusive CPU/GPU timings. Per-phase
measurement only accumulates atomic counters; the game thread emits logs.
Pinned Kazarov sources remain unchanged: matching instrumentation uses a
generated Switch-only source copy. The PC targets rebuild successfully and
all 427 ARM64 engine objects link. This is a diagnostic build, not a claimed
fix for the camera-fade slowdown.
The 39,884,997-byte NRO was uploaded alone and verified by full remote SHA-256
readback and size. SHA-256: `30570901b8f546f5fb9ea1ce2dc4ac77997922b8f1ce34ab99b5ea217c90f2e9`.
Receipt: `port/build/switch/game/app-upload-verification-0.7.6.json`.

## Preview 0.7.7: reuse native-rate images

The requested 0.7.6 hardware log pull is retained under
`port/build/switch/hardware-pulls/transition-0.7.6-20261105T111947Z`.
It confirms repeated slow and recovered intervals. In one sustained slow
five-second interval there were 27 completed end-frame calls, 25 display
renders averaging 186,417 us, and 26 queue-present calls averaging 171,982 us.
Canonical submission averaged 3,889 us, fence waits 33 us, acquisition 244 us,
CPU interpolation 2,417 us, and mesh matching 921 us. Another slow interval
had queue-present average 204,267 us. The previously suspected CPU mesh
matching cost therefore does not explain the roughly 200 ms frame time.
These CPU-side call timings locate the wait in presentation; they do not
separate GPU execution from the driver's presentation synchronization.

The large stalls correlate with the extra scene-render/display path. Intervals
dominated by presenting the already completed canonical image instead show
roughly 24–29 FPS and queue-present means around 20–24 ms. This motivates
avoiding an unnecessary second scene pass at the user's 30 FPS target rather
than changing the matcher based on the earlier hypothesis.

On Switch's synchronous path, 0.7.7 uses the completed canonical image whenever
the requested display cap is at or below the game's native image rate
(`field_rate / frame_fields`) and no mouse display update is needed. Thus
the common one/two-field camera transitions at 60 Hz and a 30 FPS cap avoid
an extra scene render. Higher display rates, an uncapped display, unknown
cadence, slower native image streams, and mouse display updates retain the
existing intermediate-frame path. The PC behavior is unchanged. This does
not claim to repair NVK presentation synchronization or establish hardware
performance before a repeat of the failing transition.

All ten presentation timing tests pass, including one/two-field transition
eligibility and the slower-image-stream fallback. PC game/test targets build,
the changed source compiles with the actual optimized ARM64 command, and all
427 engine objects link. Existing frame diagnostics remain enabled for the
hardware comparison.
The 39,884,997-byte NRO was uploaded alone and verified by size and complete
remote SHA-256 readback: `1e0f5f5aa61d224042406ae612f73a7af5992e7e721ef023317e53c680e21297`.
Receipt: `port/build/switch/game/app-upload-verification-0.7.7.json`.

### NVK presentation timing scope

The matching public nxvk branch's
[`wsi_switch.c`](https://github.com/PalindromicBreadLoaf/nxvk/blob/96e6e7bb05bdd9ca2fb6055b730e7ce36ed3e03d/src/vulkan/wsi/wsi_switch.c#L431)
shows that Switch presentation waits for an internal WSI fence before queuing
the scanout buffer. That internal function-pointer call bypasses the game's
static Vulkan wrappers, so it contributes to `queue-present` time but not the
reported `fence` phase. Short engine fence calls therefore do not establish
that GPU execution is cheap. The long presentation call can include GPU
completion as well as window queuing; the current diagnostics do not split
those two internal stages. With zero-copy enabled, this path also skips the
CPU swizzle fallback. No driver synchronization was removed: image readiness
is required for correct presentation. Source/provenance is saved under
`.kazarov/switch/nvk-memory-source/wsi-switch-source.json`.

## Preview 0.7.8: spectrumised-item icon crash

The user reported an error while creating a spectrumised item. The requested
log pull is retained under
`port/build/switch/hardware-pulls/error-0.7.7-20261105T113845Z`. The latest
Atmosphere crash report, `01793879130_01002ff008c24000.log`, reports a data
abort at module offset `0x24d554`. Its module build ID
`495ef2c5b2f42dfe140dc984bac09f7ac3e3043d` matches the preserved 0.7.7 ELF.
Symbolization resolves the PC/LR to `DrawItemIconEffect2`; the caller resolves
to `MenuItemModeItemDraw`. Disassembly shows the faulting load advances through
the local UV table, with its pointer reaching the main stack's upper boundary.

The recovered function declares `float corners[4][2]` and then uses
`corners[0][n]` for `n = k * 2`, reading outside the first two-element row.
GCC's optimized ARM64 output removes the inner loop's exit check after those
undefined accesses, causing unbounded UV reads and stack writes. The Switch
source-generation fix uses one `float corners[8]` table, retaining the same
eight coordinate values, rotation, UV order and four vertices per sparkle.
Recovered PS2 source and PC generated output remain unchanged. Fixups require
each exact source seam once, and the ARM64 audit records the generated unit's
source/stub/generator dependencies for reuse checks.

The actual corrected generated function is compiled and run by
`scripts/switch/test_icon_effect.py`. It verifies zero through five active
sparkles, exact vertex and UV sequences, draw/flush counts and null inputs,
with a five-second execution bound. The test also checks that PC output is
unchanged. All four existing source-generation tests pass. The corrected full
unit compiles with the production optimized ARM64 command, and all 427 engine
objects link. Creating a spectrumised item still needs a hardware retest.
The new ELF's disassembly also restores the inner loop's compare and conditional
backedge after `Vertex`, followed by `Flush`; the crashed ELF instead branched
unconditionally. The corrected function disassembly is retained as
`port/build/switch/game/icon-effect-disassembly-0.7.8.txt`.
Running the same test with `--sanitize` also passes under UBSan. Its negative
control substitutes the original recovered function into the same harness and
requires an out-of-bounds diagnostic; the original fails while the corrected
flat table passes. The diagnostic is retained under
`port/build/switch-icon-effect-tests/original-ubsan.txt`.
The 39,884,997-byte 0.7.8 NRO was uploaded alone and verified by remote size
and complete SHA-256 readback: `f192b062ddfa319e2af00796908b5ec82bfe5973e4c9abf8168771c633e33e9e`.
Receipt: `port/build/switch/game/app-upload-verification-0.7.8.json`.
The user subsequently confirms that creating a spectrumised item works on
0.7.8. The completed run's logs are retained under
`port/build/switch/hardware-pulls/retest-20261105T120110Z`. A listing of the
system crash directory showed no report newer than the preserved 0.7.7 crash.
This validates the reported spectrumise action; it is not exhaustive coverage
of inventory actions or a proof that all crashes are resolved.

The same run provides a clear light-scene pacing example: one five-second
window completed 298 end-frame calls but presented only 100 canonical images,
with canonical submission averaging 2,415 us and queue-present averaging
4,872 us. Thus the roughly 20 FPS display plateau was not a 20 FPS canonical
throughput limit in that window. This supports testing the 0.7.9 slot policy.

The 0.7.7 performance run contains 635 FPS samples, 450 at or above 20 FPS and
11 below 10 FPS, with a maximum of 28.9 FPS. None of the 131 timing windows
records an extra display render or CPU matching call. Queue-present's largest
window mean is 63,907 us and largest recorded call is 71,224 us; the earlier
sustained 170–205 ms extra-pass waits are absent from this run. This supports
the intended path change, but it is not a controlled stock-clock benchmark
or proof of steady 30 FPS. Heavier scenes and presentation pacing still need
optimization.

## Preview 0.7.9: canonical presentation cadence

The remaining 15 FPS plateaus expose a separate pacing problem. A strict
deadline one full period after the previous actual display start can discard
every other image from a nominally 30 Hz source with small timing variations.
Additionally, the synchronous canonical branches marked a tick shown even
when its presentation was skipped or failed, suppressing a later retry.

On Switch's native-image/no-mouse path, canonical images now use a target-slot
pacer. Successful presentations consume slots; skipped/failed attempts do not.
The ordinary pacer still records every actual start, and mode transitions
reset/check the native phase to avoid using stale deadlines. Interpolated,
mouse-driven and unbounded paths retain the ordinary policy. Tick-shown
success bookkeeping is corrected only for Switch. No game-clock or canonical
rendering work is skipped.

This is a nominal display-rate cap, rather than a requirement that every pair
of actual starts be separated by at least 33.333 ms. Nearest-slot assignment
allows bounded timing compensation, with a half-period minimum spacing.
Missing a full slot resets phase so a long stall creates no overdue display
debt. No arbitrary comparison epsilon is used.

All 17 integrated presentation tests pass, including jittered 30/60 Hz sources,
one/two-field cadence transitions, failures/retries, long stalls, mode handoff,
rate changes and clock-range edges. The PC game/tests build successfully;
the changed source compiles under the actual release ARM64 command and all
427 engine objects link. This build also includes the spectrumised-item fix.
Hardware pacing and spectrumise retests remain pending.
The first upload disconnected; the same immutable staged NRO was resumed with
the dedicated helper, then verified by exact size and complete remote SHA-256
readback. Size: 39,884,997 bytes. SHA-256:
`2516029c23d5fc095d89c74f64959c6986a61c86349f9578c6318e34541ee809`.
Receipt: `port/build/switch/game/app-upload-verification-0.7.9.json`.

### 0.7.9 hardware playback and lingering particles

The user reports 0.7.9 is very playable. Its completed logs are retained under
`port/build/switch/hardware-pulls/particle-logs-20261105T122529Z`. They include
942 FPS samples, 259 at or above 29 FPS, with slower heavy scenes and transition
stalls still present. A one-field timing window completed 291 end-frame calls
and 150 canonical presentations, with following samples around 30 FPS. This
demonstrates the intended pacing change from the earlier one-field 20 FPS
plateau, without establishing steady 30 FPS throughout the game.

The user also reports particles that remain in the room as the camera moves.
The requested latest three Album captures are retained under
`port/build/switch/hardware-pulls/latest-three`: 2026110504551300 (castle shards),
2026110504570700 (blue energy with rectangular boundaries), and
2026110507212000 (circus smoke). The first two filenames predate 0.7.9's session;
the captures must not all be attributed to the newest build. Still images do
not establish the duration of the effects or prove a common cause.

Read-only source review found ordinary particles expire in `CEffect::Step`
and billboard batches clear on `BeginCreatePacket`. Focused GCC bounds/loop
audits of the three recovered effect units compiled without the earlier menu
loop defect; an uninitialized-count warning remains a candidate only if an
invalid random-mode enum is supplied. No speculative lifetime or blending fix
has been applied.

## Preview 0.7.10: live effect-state diagnostic

Switch's game-thread pump now writes and closes `game/effect-state.txt` every
five seconds. It samples scene effect-script states, program/end/PC fields,
texture names, visible sprite counts, opacity ranges and a few world-space
positions/scales. Work is bounded to 32 scene slots, 64 linked nodes per slot,
16 detailed scripts, 256 sprites per detailed script and three sprite samples.
This covers scripted effects; it does not inspect every ordinary particle pool.
The file can be read over FTP without needing the main gameplay logs closed.
The diagnostic observes state and does not change lifetime, emission or blending.

The Switch source units compile with the production release commands and all
427 engine objects link. The PC game and tests rebuild successfully; snapshot
code and its call site are guarded by `__SWITCH__`. The lingering-particle cause
and effect-state snapshots still require a hardware reproduction.
The 39,889,093-byte NRO was uploaded alone and verified by exact size and
complete remote SHA-256 readback: `2bae83ac793d1af609f7d7a1d24c1147b318345bc149dfd90ebab7c02009230f`.
Receipt: `port/build/switch/game/app-upload-verification-0.7.10.json`.


## Window-break persistence test: 0.7.11

The seven Album captures `2026110516394000` through `2026110516400000`
show the white window-break cloud, glass and larger fragments persisting in
world space for about 20 seconds while characters and camera continue moving.
Retrieved originals: `port/build/switch/hardware-pulls/latest-seven-20261105T213329Z`.
This supports a frozen effect/update or retained draw-packet investigation;
it does not establish which path is responsible.

The 0.7.10 live snapshots showed no scripted effects after the transition.
That diagnostic missed ordinary particle pools and replaced its previous
snapshot, so it cannot establish that the visible effect was cleaned up.

Diagnostic preview 0.7.11 observes ordinary manager Step/Draw calls in generated
Switch-only source copies. It copies bounded samples of age, lifetime, opacity
and position, alongside cumulative call counts and last-seen frame numbers.
It retains at most 64 manager records, scans at most 512 particles per sample,
and never dereferences retained pool/manager pointers when reporting. A call
count records entry into Step, including early returns; it is not proof each
particle advanced. Samples are refreshed at most twice per second per manager.

Both scripted and ordinary observations are written every two seconds, closed
for live FTP access, and retained in 60 rotating `effect-history-NN.txt` slots
(two minutes). Each includes session and sequence identifiers, so old slots
from another run can be distinguished. `scripts/switch/pull_effect_history.py`
retrieves and orders these read-only; ordinary stdout/stderr can remain locked
while running. Play through the window break, then close normally and request
a pull within two minutes of gameplay. No timed pause is required.

The observer's actual code passes an optimized UBSan host harness covering
scan bounds, ageing samples, update/draw counts, pool reuse, reporting after
pool storage expires, and bounded record eviction. The complete 427-object
ARM64 release engine links with zero unresolved or duplicate symbols.
NRO-only deployment to `/switch/ChronicleTwo-PC-Switch` has matching remote
size and SHA-256 readback (`app-upload-verification-0.7.11.json`).
The PC game and test targets also rebuilt successfully (30.8 seconds); the
existing source-generation checks pass 4/4. This is a diagnostic build; no
particle cleanup fix is claimed yet.


## Archived-build comparison

The user recalls this same window-break effect clearing in the 0.7.1�0.7.3
period and initially reported it present by 0.7.8. The user subsequently
retested the archived builds and confirmed it happens in all of them, retracting
the regression window. Investigate the shared animation/effect path. Material changes in this interval include
release optimization (0.7.3), audio/loading integration (0.7.4), canonical
presentation capping (0.7.5) and reuse instead of an extra display pass (0.7.7).
The 0.7.8 source fix concerns item-icon sparkle bounds, not this window effect.
Neither optimization nor the presentation change is established as the cause.

For the user's requested comparison, eight original archived NROs (0.7.1 through
0.7.8) are staged under `port/build/switch/game/regression-comparison-0.7.1-0.7.8`.
Each is checked against its archived build manifest and named
`ChronicleTwo-0.7.N.nro` for separate installation in the dedicated app folder.
They retain their original embedded versions and share the existing absolute
game run directory; no assets, saves or config are copied for the comparison.
The current `chronicletwo-switch.nro` is retained. Deployment verification is
recorded in `port/build/switch/game/regression-comparison-verification.json`.
All eight remote files now match the original archived sizes and full SHA-256
hashes; the current build was not replaced.
Compare the same window sequence, closing completely between versions, and
record the last version that clears and the first that freezes. Normal logs
are shared and overwritten by the next launch, so retrieve any needed run's
logs before starting the following version.


## Preview 0.7.12: held material fades

The user's archived-build comparison confirms the lingering effect occurs in
all tested versions (0.7.1�0.7.8). The 0.7.11 history contains 60 snapshots
but no ordinary manager calls or live scripted effects in the captured run;
it does not establish cleanup of the visible window effect.

The prepared `event/e00/e50/e50_glassware.gltf` bundle and its sidecar identify
a named `shatter` model animation with duration 58 frames. Its 144 auxiliary
material-transparency channels include 140 ending at transparency 1 (opacity
0); four retain their authored nonzero opacity. This is animated geometry,
not an ordinary particle pool. Extracted local inspection artifacts are under
`port/build/switch-particle-audit`; no game assets are committed or modified.

Native character actions previously sent these auxiliary channels through the
recovered `MotionProc`. At/after its last key, that evaluator chooses the same
key for both endpoints but divides by their zero frame distance. Material-alpha
interpolation then writes NaN instead of the final opacity. A single-key curve
also divides by zero; sampling before a delayed first key can use index -1.
The new regression tests reproduce NaN opacity before the fix (and the legacy
same-action blend test faults), as retained in `.kazarov/switch-fade-before-tests.log`.
This proves a boundary bug, not yet that it explains every visible particle.

`character_motion.cpp` now samples auxiliary material transparency with bounded
endpoints in ordinary poses, same-action blends, and target auxiliary channels
of cross-action blends. It holds the exact first/last value and handles one-key
curves without division. Intermediate values retain linear interpolation and
transparency-to-opacity conversion. Other channel types retain their existing
paths; the recovered source and game data are unchanged. This fixes a shared
native runtime bug, so PC and Switch use the correction.

All 45 focused fade/native-model tests pass after the fix, including ordinary
poses, held/single keys, same-action blends and cross-action target fades. The PC game and test
targets rebuild successfully, the real ARM64 motion object compiles, and all
427 release objects link with zero unresolved or duplicate symbols. Preview
0.7.12 packages the change with the existing pacing, audio, save, loading and
history support. Hardware replay of the window-break sequence remains required
before claiming the lingering glass/cloud is fixed.

The 0.7.12 NRO is uploaded and verified by remote size and full SHA-256 readback
(`app-upload-verification-0.7.12.json`): 39,889,093 bytes,
`a1dd915b0e2c20ad97ab290402fb8c68aae34ad12d3e4e9e4b3002049167c300`.
