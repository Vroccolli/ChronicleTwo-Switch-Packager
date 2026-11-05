"""Apply the pinned homebrew backend, then attach ChronicleTwo's Vulkan WSI."""
from pathlib import Path
import subprocess
import os

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / ".kazarov/switch/deps/SDL-release-3.4.18"
BACKEND = ROOT / ".kazarov/switch/deps/sdl3-switch-182e511214d7600e4bdab8606d7caf0ef744afd6"


def replace(path, before, after):
    text = path.read_text()
    # Some replacements retain the original seam. Normalize any earlier
    # expansions before applying once, including older duplicated insertions.
    original = text
    while after in text:
        text = text.replace(after, before)
    if before not in text:
        raise ValueError(f"SDL seam changed: {path}")
    if text.count(before) != 1:
        raise ValueError(f"Ambiguous SDL seam: {path}")
    text = text.replace(before, after)
    if text != original:
        path.write_text(text)


def main():
    driver = SOURCE / "src/video/switch/SDL_switchvideo.c"
    if not driver.exists():
        git = "C:/Program Files/Git/cmd/git.exe" if os.name == "nt" else "git"
        subprocess.run([git, "apply", "-p3", "--directory=" + SOURCE.relative_to(ROOT).as_posix(),
            str(BACKEND / "sdl3-switch.patch")], cwd=ROOT, check=True)
    replace(SOURCE / "cmake/sdlplatform.cmake", 'elseif(CMAKE_SYSTEM_NAME MATCHES "NintendoSwitch")',
        'elseif(CMAKE_SYSTEM_NAME MATCHES "NintendoSwitch" OR CT_SWITCH_PLATFORM)')
    replace(SOURCE / "CMakeLists.txt", 'ANDROID OR APPLE OR LINUX OR FREEBSD OR OPENBSD OR WINDOWS" OFF)',
        'ANDROID OR APPLE OR LINUX OR FREEBSD OR OPENBSD OR WINDOWS OR SWITCH" OFF)')
    replace(SOURCE / "CMakeLists.txt", 'elseif(SWITCH)\n', 'elseif(SWITCH)\n  set(SDL_VIDEO_VULKAN 1)\n')
    replace(driver, '#include "SDL_switchvideo.h"',
        '#include "SDL_switchvideo.h"\n\n// ChronicleTwo static-Vulkan adapter.\nextern void ctSdlAttachVulkan(SDL_VideoDevice *video);')
    replace(driver, '    device->device_caps = VIDEO_DEVICE_CAPS_FULLSCREEN_ONLY;',
        '    ctSdlAttachVulkan(device);\n    device->device_caps = VIDEO_DEVICE_CAPS_FULLSCREEN_ONLY;')
    # Newlib declares fdatasync but libsysbase implements fsync. Use the real
    # devoptab flush path rather than supplying a success-returning shim.
    replace(SOURCE / "src/io/SDL_iostream.c", '#elif defined(SDL_PLATFORM_HAIKU)',
        '#elif defined(SDL_PLATFORM_HAIKU) || defined(SDL_PLATFORM_SWITCH)')
    # SDL's a/b/x/y mapping slots mean south/east/west/north. The backend
    # uses Nintendo's letter order (A, B, X, Y), so preserve PS2 positions:
    # B -> Cross, A -> Circle, Y -> Square, X -> Triangle.
    replace(SOURCE / "src/joystick/switch/SDL_sysjoystick.c",
        '    out->a = (SDL_InputMapping){ EMappingKind_Button, 0 };\n'
        '    out->b = (SDL_InputMapping){ EMappingKind_Button, 1 };\n'
        '    out->x = (SDL_InputMapping){ EMappingKind_Button, 2 };\n'
        '    out->y = (SDL_InputMapping){ EMappingKind_Button, 3 };',
        '    out->a = (SDL_InputMapping){ EMappingKind_Button, 1 };\n'
        '    out->b = (SDL_InputMapping){ EMappingKind_Button, 0 };\n'
        '    out->x = (SDL_InputMapping){ EMappingKind_Button, 3 };\n'
        '    out->y = (SDL_InputMapping){ EMappingKind_Button, 2 };')
    print(SOURCE)


if __name__ == "__main__":
    main()
