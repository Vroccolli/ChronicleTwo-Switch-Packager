# PC-to-Switch work plan

## Objective

Cross-compile the existing native ChronicleTwo PC engine and recovered gameplay
source for Nintendo Switch ARM64. Preserve the game's MainLoop, scene system,
scripts, combat, inventory and saves where the native PC implementation supports
them. Adapt platform boundaries instead of creating a separate gameplay engine.
Missing or incorrect PC functionality remains a separate, explicit limitation.

## Provenance

Created on 2026-11-04 from locally available upstream ChronicleTwo revision
`cb910edeea375887aab8e00085b93150be4e081e`, with Kazarov pinned at
`14164f13fb4fcd4395009b81ba3f3208df1e52b3`. Original Git history is retained.
No uncommitted changes from the existing Switch prototype were imported.
The `upstream` remote identifies the original ChronicleTwo repository; no
publishing remote is configured for this new project.

## Implementation boundaries

- Keep the native PC target available as the reference implementation.
- Preserve the existing PORT_OVERRIDE and generated-source pipeline.
- Add a Switch target sharing the PC game's source list and game entry path.
- Put Switch platform implementations behind explicit build selections.
- Adapt graphics, audio output, controls, filesystem, threading and memory.
- Evaluate deko3d against the existing Kazarov renderer contract before choosing
  the backend design; the PC Vulkan backend cannot simply be assumed to work.
- Record compiler and dependency constraints, including C++26, SDL3, shader
  tooling, movie decoding and script runtime requirements.
- Keep retail disc images, extracted data, saves and build outputs out of Git.

## Milestones and acceptance

1. Establish the PC baseline. Build the pinned PC target, record the toolchain
   and actual boot/gameplay boundaries, and run its relevant existing checks.
2. Audit ARM64 compilation. Reuse the PC source-generation pipeline, compile
   the shared game units, and record platform and dependency failures. Compilation
   of a subset is not acceptance of the full engine.
3. Connect Switch services. Implement the required memory, file, input, audio,
   timing and thread interfaces. Verify them on hardware with small diagnostics.
4. Connect rendering. Implement the renderer contract on Switch, checking
   textures, skinned meshes, render targets, shaders and frame submission.
5. Boot the shared game. Reach title and New Game through the existing MainLoop
   and normal scene loading. Record the build revision and hardware results.
6. Validate gameplay incrementally against the PC baseline: opening, town,
   dungeon, combat, menus, saves and loads. Investigate divergences in shared
   code or adapters instead of building parallel gameplay systems.

## Current status

Only repository initialization and planning are complete. No Switch compiler
audit, PC baseline run, full engine build or hardware validation has been done
in this new checkout. The first implementation task is milestone 1.
