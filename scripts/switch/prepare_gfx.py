"""Generate Switch GPU memory adaptation and display-list timing copies."""
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


def prepare_displaylist(output):
    source=ROOT/'tools/kazarov/src/cpp/gfx/displaylist.cpp'
    text=source.read_text()
    text='#include "gfx/frame_timing.hpp"\n'+text
    for function, phase in [('BuildMatches','Matching'),('Interpolated','Interpolation')]:
        seam=f'    KZ_PROFILE_ZONE("Kazarov {function}");'
        if text.count(seam)!=1:raise RuntimeError('Display-list timing seam changed: '+function)
        text=text.replace(seam,seam+f'\n    ct::frame_timing::Scope timing(ct::frame_timing::Phase::{phase});')
    output=Path(output);output.mkdir(parents=True,exist_ok=True)
    generated=output/'displaylist.cpp';generated.write_text(text)
    return generated


def prepare_overlay(output):
    """Keep the pinned font renderer, adding an exact-size bottom-right FPS entry."""
    source = ROOT / 'tools/kazarov/src/cpp/platform/overlay.cpp'
    text = source.read_text()
    text += r"""

void ctSwitchDrawFps(std::string_view text) {
    const auto mapping = gfx::GetUiMapping(gfx::kMainTarget);
    const int pixel = OverlayPixelSize(gfx::GetLogicalMapping(gfx::kMainTarget));
    int columns = 0;
    for (char c : text) {
        if (const auto *glyph = FindFontGlyph(c)) columns += glyph->advance;
    }
    const int margin = pixel * kOverlayPadding;
    const int width = (columns + 2 * kOverlayPadding) * pixel;
    const int height = (kFontHeight + 2 * kOverlayPadding) * pixel;
    const int x = std::max(margin, static_cast<int>(mapping.pixel_width) - width - margin);
    const int y = std::max(margin, static_cast<int>(mapping.pixel_height) - height - margin);
    OverlayDrawText(text, mapping, pixel, x, y);
}
"""
    output = Path(output)
    output.mkdir(parents=True, exist_ok=True)
    generated = output / 'overlay.cpp'
    generated.write_text(text)
    return generated
