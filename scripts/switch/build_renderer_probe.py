"""Link the unchanged real engine renderer to SDL3/libnx/NVK for hardware tests."""
from pathlib import Path
import hashlib
import json
import os
import subprocess
from audit_compile import ROOT
from prepare_vulkan_dispatch import prepare_dispatch


def main():
    sdk = Path("C:/devkitPro")
    out = ROOT / "port/build/switch/renderer-probe"
    out.mkdir(parents=True, exist_ok=True)
    audit = ROOT / "port/build/switch-engine/objects"
    report = json.loads((audit / "report.json").read_text())
    objects, sources, commands = [], [], []
    for entry in report["results"]:
        name = entry["original_source"]
        if name.startswith("tools/kazarov/src/cpp/gfx/") or name == "tools/kazarov/src/cpp/platform/log.cpp":
            for dependency in entry.get("dependencies", []):
                if hashlib.sha256((ROOT / dependency["path"]).read_bytes()).hexdigest() != dependency["sha256"]:
                    raise RuntimeError("Stale source dependency: " + dependency["path"])
            source = ROOT / entry["source"]
            if entry["exit_code"] or hashlib.sha256(source.read_bytes()).hexdigest() != entry["source_sha256"]:
                raise RuntimeError(f"Stale/failed engine object: {name}")
            objects.append(audit / entry["log"].replace(".log", ".o"))
            sources.append(dict(path=entry["source"],sha256=entry["source_sha256"]))
    sdl = ROOT / ".kazarov/switch/deps/SDL-release-3.4.18"
    vulkan = next((ROOT / ".kazarov/windows/deps/vulkan").glob("Vulkan-Headers-*/include"))
    common = ["-march=armv8-a+crc+crypto", "-mtune=cortex-a57", "-mtp=soft", "-fPIE",
        "-ffunction-sections", "-fdata-sections", "-fsigned-char", "-ffp-contract=off", "-D__SWITCH__", "-DNDEBUG",
        "-I"+str(sdk / "libnx/include"), "-I"+str(vulkan), "-I"+str(ROOT / "tools/kazarov/include"),
        "-I"+str(sdl / "include")]
    env = dict(os.environ,DEVKITPRO=sdk.as_posix())
    def run(command):
        commands.append([str(x) for x in command]); subprocess.run(commands[-1],env=env,cwd=ROOT,check=True)
    dispatch = prepare_dispatch(out / "generated")
    for source in [ROOT / "port/switch/renderer_probe/platform.c", ROOT / "port/switch/renderer_probe/main.cpp", dispatch]:
        compiler = sdk / "devkitA64/bin" / ("aarch64-none-elf-gcc.exe" if source.suffix == ".c" else "aarch64-none-elf-g++.exe")
        obj = out / (source.stem+".o")
        run([compiler,"-std=gnu11" if source.suffix == ".c" else "-std=gnu++26","-O2",*common,"-c",source,"-o",obj])
        objects.append(obj); sources.append(dict(path=source.relative_to(ROOT).as_posix(),sha256=hashlib.sha256(source.read_bytes()).hexdigest()))
    library = ROOT / ".kazarov/switch/deps/nvk-25.0.7/lib/libvulkan.a"
    if hashlib.sha256(library.read_bytes()).hexdigest() != "ff65a74b46fe749f6be73eac3c8968d70e312c7646f815f26b69f4aa35737d26":
        raise RuntimeError("Unexpected driver library")
    sdl_library = ROOT / "port/build/switch-sdl3-vulkan/sdl/libSDL3.a"
    elf=out/"chronicletwo-renderer-probe.elf"
    run([sdk/"devkitA64/bin/aarch64-none-elf-g++.exe",*common,
        "-specs="+str(sdk/"libnx/switch.specs"),"-L"+str(sdk/"libnx/lib"),"-L"+str(sdk/"portlibs/switch/lib"),
        "-Wl,--gc-sections","-Wl,-Map,"+str(out/"probe.map"),"-o",elf,*objects,
        "-Wl,--start-group",sdl_library,library,"-lEGL","-lGLESv2","-lglapi","-ldrm_nouveau","-lz","-lzstd","-lnx","-lm","-Wl,--end-group"])
    nacp=out/"probe.nacp"; nro=out/"chronicletwo-renderer-probe.nro"
    run([sdk/"tools/bin/nacptool.exe","--create","ChronicleTwo real renderer probe","ChronicleTwo contributors","0.6.1",nacp])
    run([sdk/"tools/bin/elf2nro.exe",elf,nro,"--nacp="+str(nacp),"--icon="+str(sdk/"libnx/default_icon.jpg")])
    (out/"build.json").write_text(json.dumps(dict(sources=sources,commands=commands,
        objects=[dict(path=str(p),sha256=hashlib.sha256(p.read_bytes()).hexdigest()) for p in objects],
        libraries=[dict(path=str(p),sha256=hashlib.sha256(p.read_bytes()).hexdigest()) for p in [library,sdl_library]],
        nro_sha256=hashlib.sha256(nro.read_bytes()).hexdigest(),hardware_tested=False,full_engine=False),indent=2))
    print(f"Built {nro}: {nro.stat().st_size} bytes")


if __name__ == "__main__":
    main()
