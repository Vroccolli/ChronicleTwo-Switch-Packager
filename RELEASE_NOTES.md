# Packager v0.1.1 preview

Updated standalone Windows x64 packager, containing native Switch engine 0.7.13.

Replaces the bundled 0.7.10 NRO with the complete 427-object 0.7.13 engine build.
The ISO extractor, matching gameplay seed and controller artwork are unchanged.
The new NRO matches its build report and the recorded Switch upload checksum.
The earlier full-disc extraction verification remains applicable to the unchanged
extractor and seed; gameplay execution of 0.7.13 is not established by these checks.

- Select a supported PAL ISO and an output directory through the GUI.
- No Python, development tools, network or PC game installation needed.
- Verify the ISO, bundled runtime, converted archive and final installation.
- Preserve existing installations and stage failures separately.
- Include controller artwork, a 30 FPS default and dependency notices.

Tested end to end with the European PAL ISO. Eight workflow tests pass. This
engine build is not hardware-tested; performance and compatibility work continues.

Extract the whole ZIP before running the EXE. Allow 12 GiB free and several GiB of
RAM. The US Dark Cloud 2 ISO is not supported. No retail game data is included.
