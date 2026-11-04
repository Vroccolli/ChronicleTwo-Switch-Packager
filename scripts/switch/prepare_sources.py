"""Prepare GCC build copies without changing recovered sources or PC output."""
import hashlib
import importlib.util
import json
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location("port_override", ROOT / "tools/kazarov/scripts/build/port_override.py")
parser = importlib.util.module_from_spec(spec)
spec.loader.exec_module(parser)


def exported_names(header):
    # Stub headers explicitly name the private storage/functions shared with
    # native overrides. Keep their asm labels and make only those names external.
    return set(re.findall(r'\b(\w+)\s*(?:\([^;]*?\)|(?:\[[^\]]*\]\s*)*)\s+asm\s*\(', header))


def externalize(text, names):
    flat, _, _ = parser.flatten(parser.mask(text))
    edits, depth = [], 0
    for token in re.finditer(r"\bstatic\b|[{}]", flat):
        word = token[0]
        if word == "{":
            depth += 1
        elif word == "}":
            depth -= 1
        elif depth == 0:
            definition = parser.Definition(text[token.start():], flat[token.start():], function_declarations=True)
            if definition.spelled in names:
                edits.append((token.start(), definition.spelled))
    for offset, _ in reversed(edits):
        text = text[:offset] + " " * 6 + text[offset + 6:]
    return text, edits


def prepare_source(source, output):
    header = ROOT / "port/include/stubs" / (source.stem + ".hpp")
    if not header.exists():
        return source
    original = source.read_text(encoding="latin-1")
    names = exported_names(header.read_text())
    # mainloop itself declares this object extern before its static definition.
    if source.name == "mainloop.cpp":
        if "extern mgCMemory MainBuffer;" not in original:
            raise ValueError("MainBuffer's existing external declaration changed")
        names.add("MainBuffer")
    text, edits = externalize(original, names)
    fixups = {
        # cursor is an int, not storage for an eight-byte native pointer.
        "menusys.cpp": [("(int) *(void **) &MenuCommonInfo->cursor", "MenuCommonInfo->cursor")],
        # Preserve the exact byte values with explicit signed-char conversion.
        "menuaqua.cpp": [("0x05, 0xF9, 0x01, 0xBB, 0x03", "0x05, static_cast<char>(0xF9), 0x01, static_cast<char>(0xBB), 0x03")],
    }.get(source.name, [])
    for before, after in fixups:
        if text.count(before) != 1:
            raise ValueError(f"{source}: expected one occurrence of {before}")
        text = text.replace(before, after)
    if not edits and not fixups:
        return source
    output.mkdir(parents=True, exist_ok=True)
    generated = output / source.name
    generated.write_bytes(text.encode("latin-1"))
    generated.with_suffix(".json").write_text(json.dumps(dict(
        original=str(source), original_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),
        stub_sha256=hashlib.sha256(header.read_bytes()).hexdigest(),
        generated_sha256=hashlib.sha256(generated.read_bytes()).hexdigest(),
        externalized=[dict(offset=offset, name=name) for offset, name in edits],
        fixups=fixups), indent=2))
    return generated
