"""Adapt only Switch GPU memory selection in a generated Kazarov source copy."""
from pathlib import Path
import hashlib
import json

ROOT=Path(__file__).resolve().parents[2]

def prepare_memory(output):
    source=ROOT/'tools/kazarov/src/cpp/gfx/memory.cpp'
    text=source.read_text()
    seam='    Fatal("no memory type with properties 0x%x", required);'
    if text.count(seam)!=1:raise RuntimeError('GPU allocator selection seam changed')
    text='#include "gfx/memory_policy.hpp"\n'+text
    text=text.replace(seam,'''    if (auto type=ct::gfx::SharedLocalType(properties,type_bits,required,
            g.properties.vendorID,g.properties.deviceType)) {
        static bool reported=false;
        if (!reported) {
            kz::Logf(kz::LogLevel::Info,"gfx",
                "Switch shared local heap: type {}, flags {:#x}, heap {}, compatible mask {:#x}",
                *type,properties.memoryTypes[*type].propertyFlags,
                properties.memoryTypes[*type].heapIndex,type_bits);
            reported=true;
        }
        return *type;
    }
    kz::Logf(kz::LogLevel::Error,"gfx","memory selection: {} types, compatible mask {:#x}",
        properties.memoryTypeCount,type_bits);
    for (uint32_t i=0;i<properties.memoryTypeCount;++i)
        kz::Logf(kz::LogLevel::Error,"gfx","type {}: flags {:#x}, heap {}",i,
            properties.memoryTypes[i].propertyFlags,properties.memoryTypes[i].heapIndex);
'''+seam)
    output=Path(output);output.mkdir(parents=True,exist_ok=True)
    generated=output/'memory.cpp';generated.write_text(text)
    (output/'memory-source.json').write_text(json.dumps(dict(original=str(source),
        original_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),
        generated_sha256=hashlib.sha256(generated.read_bytes()).hexdigest(),
        policy_sha256=hashlib.sha256((ROOT/'port/switch/gfx/memory_policy.hpp').read_bytes()).hexdigest()),indent=2))
    return generated
