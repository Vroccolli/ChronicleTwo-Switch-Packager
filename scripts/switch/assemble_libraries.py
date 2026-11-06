"""Collect only the built/acquired ARM64 libraries and static Vulkan dispatch."""
from pathlib import Path
import hashlib
import json
import os
import subprocess
from audit_compile import ROOT
from prepare_vulkan_dispatch import prepare_dispatch
from toolchain import sdk_root


def main():
    sdk=sdk_root()
    out=ROOT/"port/build/switch-libraries"
    out.mkdir(parents=True,exist_ok=True)
    source=prepare_dispatch(out/"generated")
    headers=next((ROOT/".kazarov/windows/deps/vulkan").glob("Vulkan-Headers-*/include"))
    obj=out/"vulkan_dispatch.o"; library=out/"libct_switch_vulkan.a"
    command=[str(sdk/"devkitA64/bin/aarch64-none-elf-g++.exe"),"-std=gnu++26","-O2",
        "-march=armv8-a+crc+crypto","-mtp=soft","-fPIE","-ffunction-sections","-fdata-sections",
        "-D__SWITCH__","-DNDEBUG","-I"+str(headers),"-I"+str(ROOT/"tools/kazarov/include"),
        "-I"+str(ROOT/"port/switch"),
        "-I"+str(sdk/"libnx/include"),"-c",str(source),"-o",str(obj)]
    subprocess.run(command,check=True)
    subprocess.run([str(sdk/"devkitA64/bin/aarch64-none-elf-gcc-ar.exe"),"rcs",str(library),str(obj)],check=True)
    archives=json.loads((ROOT/"port/build/switch-dependencies-native/libraries.json").read_text())["archives"]
    archives+=json.loads((ROOT/"port/build/switch-sdl3-vulkan/library.json").read_text())["archives"]
    ffmpeg=ROOT/".kazarov/switch/deps/ffmpeg-7.1-5/opt/devkitpro/portlibs/switch"
    archives += [str(ffmpeg/"lib"/name) for name in ("libavformat.a","libavcodec.a","libavutil.a","libswscale.a","libswresample.a")]
    archives += [str(library),str(ROOT/".kazarov/switch/deps/nvk-25.0.7/lib/libvulkan.a"),
        str(ROOT/".kazarov/switch/deps/dav1d-1.5.0-1/opt/devkitpro/portlibs/switch/lib/libdav1d.a")]
    archives += [str(sdk/"portlibs/switch/lib"/name) for name in
        ("libEGL.a","libGLESv2.a","libglapi.a","libdrm_nouveau.a","libz.a","libzstd.a","libbz2.a")]
    for name in archives:
        if not Path(name).is_file(): raise RuntimeError(f"Missing Switch library: {name}")
    manifest=dict(platform="NintendoSwitch",archives=archives,media_headers=str(ffmpeg/"include"),
        hashes={name:hashlib.sha256(Path(name).read_bytes()).hexdigest() for name in archives},dispatch_command=command)
    (out/"libraries.json").write_text(json.dumps(manifest,indent=2))
    print(out/"libraries.json")


if __name__ == '__main__': main()
