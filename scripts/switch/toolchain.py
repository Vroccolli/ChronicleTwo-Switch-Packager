"""Machine-independent locations for the native Windows Switch build."""
import os
from pathlib import Path
import shutil


def sdk_root():
    value = os.environ.get("DEVKITPRO", "C:/devkitPro")
    # devkitPro's MSYS profile exports a virtual mount, not a native Windows path.
    if os.name == "nt" and value.lower() == "/opt/devkitpro" and Path("C:/devkitPro").is_dir():
        value = "C:/devkitPro"
    return Path(value).resolve()


def find_tool(name, variable, candidates=()):
    configured = os.environ.get(variable)
    choices = [configured] if configured else [*candidates, shutil.which(name)]
    for choice in choices:
        if choice and Path(choice).is_file():
            return str(Path(choice).resolve())
    raise RuntimeError(f"Cannot find {name}; set {variable} to its executable path")


def cmake_path():
    programs = Path(os.environ.get("ProgramFiles", "C:/Program Files"))
    return find_tool("cmake", "CT_CMAKE", (programs / "CMake/bin/cmake.exe", "C:/Strawberry/c/bin/cmake.exe"))


def git_path():
    programs = Path(os.environ.get("ProgramFiles", "C:/Program Files"))
    return find_tool("git", "CT_GIT", (programs / "Git/cmd/git.exe",))
