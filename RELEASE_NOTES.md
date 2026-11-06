# Packager v0.1.2 preview — native engine 0.9.1

Updated standalone Windows x64 packager with the complete native 0.9.1 NRO.

- Includes the new moon launcher icon and author credit to The MoonPeople and
  ChronicleTwo contributors, embedded in the NRO.
- Includes 0.9.0's FPS-only bottom-right overlay and the earlier engine fixes.
- The NRO matches its complete 427-object build report and verified Switch upload.
- The ISO extractor, matching gameplay seed and controller artwork remain unchanged.
- Full engine source and the Windows fresh-checkout build guide are available on
  the repository's switch-engine branch, linked from the default README.

Download ChronicleTwo-Switch-Packager.zip under Assets and extract the whole ZIP
before opening the EXE. Select your European PAL Dark Chronicle ISO and an output
folder with at least 12 GiB free. No Python, compilers, SDK or PC installation is
required to use the prebuilt app. No retail game data is distributed.

Eight packager workflow tests pass. The unchanged extraction pipeline was tested
previously with the PAL ISO; it was not rerun for this NRO update. Build/package
and transfer verification do not establish hardware gameplay testing of 0.9.1.
