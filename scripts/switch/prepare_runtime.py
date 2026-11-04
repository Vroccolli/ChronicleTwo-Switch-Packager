"""Add a platform context adapter to a build copy of Kazarov's unchanged scheduler."""
from pathlib import Path
import hashlib
import json

ROOT = Path(__file__).resolve().parents[2]


def prepare_scheduler(output: Path):
    source = ROOT / "tools/kazarov/src/cpp/sce/eekernel_threads.cpp"
    text = source.read_text()
    original_body = text[text.index("\nstruct Thread {"):]
    start = text.index("#if defined(_WIN32)\n// windows.h")
    end = text.index("\nnamespace {", start)
    platform_headers = text[start:end]
    text = text[:start] + '#if defined(__SWITCH__)\n#include "runtime/fiber.hpp"\n#else\n' + platform_headers + '#endif\n' + text[end:]
    marker = "void RunEntry();\n\n#if defined(_WIN32)"
    if text.count(marker) != 1:
        raise RuntimeError("Scheduler context boundary changed; review the upstream source")
    adapter = '''void RunEntry();

#if defined(__SWITCH__)
using Context = CtFiberContext;
void ContextInitMain(Context &) {}
void ContextCreate(Context &c) { CtFiberCreate(c, RunEntry, kStackSize); }
void ContextSwitch(Context &previous, Context &next) { ctFiberSwap(&previous.registers, &next.registers); }
void ContextDestroy(Context &c) { CtFiberDestroy(c); }
#elif defined(_WIN32)'''
    text = text.replace(marker, adapter)
    if text[text.index("\nstruct Thread {"):] != original_body:
        raise RuntimeError("Shared scheduler body changed during platform adaptation")
    output.mkdir(parents=True, exist_ok=True)
    generated = output / "eekernel_threads.cpp"
    generated.write_text(text)
    (output / "scheduler-source.json").write_text(json.dumps(dict(
        original=str(source), original_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),
        generated_sha256=hashlib.sha256(generated.read_bytes()).hexdigest(),
        adaptation="Switch context headers and four platform context functions only; scheduler body unchanged"), indent=2))
    return generated
