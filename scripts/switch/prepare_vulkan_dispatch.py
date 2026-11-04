"""Generate typed Vulkan calls to the static Switch ICD, preserving the engine API."""
from pathlib import Path
import hashlib
import json
import re

ROOT = Path(__file__).resolve().parents[2]


def prepare_dispatch(output):
    headers = next((ROOT / ".kazarov/windows/deps/vulkan").glob("Vulkan-Headers-*/include"))
    header = headers / "vulkan/vulkan_core.h"
    prototypes = {name: (result, params) for result, name, params in
        re.findall(r"VKAPI_ATTR\s+(\w+)\s+VKAPI_CALL\s+(vk\w+)\((.*?)\);", header.read_text(), re.S)}
    names = (ROOT / "scripts/switch/vulkan_commands.txt").read_text().splitlines()
    code = '''// Generated from the pinned Khronos prototypes. Do not edit.
#include "gfx/requirements.hpp"
#include "kazarov/log.hpp"
#include <atomic>
extern "C" PFN_vkVoidFunction vk_common_GetDeviceProcAddr(VkDevice, const char *);
namespace {
std::atomic<VkInstance> currentInstance{VK_NULL_HANDLE};
std::atomic<VkDevice> currentDevice{VK_NULL_HANDLE};
template<class T> T Resolve(const char *name) {
    auto fn = vkGetInstanceProcAddr(currentInstance.load(),name);
    if (!fn && currentDevice.load()) fn=vk_common_GetDeviceProcAddr(currentDevice.load(),name);
    if (!fn) kz::Abort("vulkan",name);
    return reinterpret_cast<T>(fn);
}
}
'''
    for name in names:
        if name == "vkGetInstanceProcAddr":
            continue  # The packaged loaderless shim exports this entry directly.
        result, params = prototypes[name]
        arguments = []
        for parameter in params.split(","):
            parameter = parameter.strip()
            if parameter != "void":
                match = re.search(r"(\w+)\s*(?:\[[^\]]*\]\s*)?$", parameter)
                if not match: raise ValueError(f"Cannot read {name}: {parameter}")
                arguments.append(match[1])
        code += f'extern "C" VKAPI_ATTR {result} VKAPI_CALL {name}({params}) {{\n'
        call = f'Resolve<PFN_{name}>("{name}")({", ".join(arguments)})'
        if name == "vkGetDeviceProcAddr":
            code += '    return vk_common_GetDeviceProcAddr(device,pName);\n'
        elif name in ("vkCreateInstance", "vkCreateDevice"):
            handle, state = ("pInstance", "currentInstance") if name == "vkCreateInstance" else ("pDevice", "currentDevice")
            code += f'    auto result={call};\n    if (result==VK_SUCCESS) {state}.store(*{handle});\n    return result;\n'
        elif name in ("vkDestroyInstance", "vkDestroyDevice"):
            handle, state = ("instance", "currentInstance") if name == "vkDestroyInstance" else ("device", "currentDevice")
            code += f'    {call};\n    if ({state}.load()=={handle}) {state}.store(VK_NULL_HANDLE);\n'
        else:
            code += f'    {"return " if result != "void" else ""}{call};\n'
        code += '}\n'
    output = Path(output)
    output.mkdir(parents=True, exist_ok=True)
    source = output / "vulkan_dispatch.cpp"
    source.write_text(code)
    (output / "vulkan_dispatch.json").write_text(json.dumps(dict(commands=names,
        header=str(header), header_sha256=hashlib.sha256(header.read_bytes()).hexdigest(),
        source_sha256=hashlib.sha256(source.read_bytes()).hexdigest()), indent=2))
    return source


if __name__ == "__main__":
    import sys
    print(prepare_dispatch(sys.argv[1]))
