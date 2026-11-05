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
