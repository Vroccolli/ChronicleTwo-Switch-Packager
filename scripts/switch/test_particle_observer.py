"""Run the actual Switch observer against bounded fake particle pools."""
from pathlib import Path
import os
import re
import subprocess

ROOT = Path(__file__).resolve().parents[2]
out = ROOT / "port/build/switch-particle-observer-tests"
out.mkdir(parents=True, exist_ok=True)
source = (ROOT / "port/src/effect.cpp").read_text()
observer = source.split("#ifdef __SWITCH__", 1)[1].split("#endif", 1)[0]
observer = re.sub(r'^#include.*$', '', observer, flags=re.M)
prefix = r'''
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <cstdlib>
struct CEffect { int active=0, frame=0; float alpha=0, pos[4]{}; struct { int life=0; } param; };
struct CEffectManager { char name[32]{},img_name[32]{}; CEffect *effects=nullptr; int effect_num=0,run=0; };
unsigned long long frame=1;
auto mgPortFrameCount() { return frame; }
void check(bool value) { if (!value) std::abort(); }
'''
main = r'''
int main() {
    CEffect particles[513]{};
    particles[0].active=particles[511].active=particles[512].active=1;
    particles[0].frame=5; particles[0].param.life=4;
    particles[0].alpha=std::numeric_limits<float>::quiet_NaN();
    particles[511].frame=2; particles[511].param.life=10;
    CEffectManager manager; manager.effects=particles; manager.effect_num=513;
    ctSwitchObserveParticles(&manager, false);
    auto &entry=particle_observations[0];
    check(entry.scanned==512 && entry.active==2 && entry.expired==1 && entry.nonfinite==1);
    check(entry.min_age==2 && entry.max_age==5 && entry.sample_count==2);
    check(entry.steps==1 && entry.draws==0);
    frame=2; ctSwitchObserveParticles(&manager, true);
    check(entry.steps==1 && entry.draws==1 && entry.draw_frame==2);
    // A later sample captures ageing and expiry, without altering the live pool.
    entry.sampled={}; particles[0].active=0; particles[511].frame=3;
    ctSwitchObserveParticles(&manager, false);
    check(entry.active==1 && entry.samples[0].age==3 && particles[511].frame==3);
    // Reused manager storage with a new pool resets counters and samples.
    manager.effects=nullptr; manager.effect_num=-1;
    ctSwitchObserveParticles(&manager, true);
    check(entry.scanned==0 && entry.active==0 && entry.steps==0 && entry.draws==1);
    // Retained records contain copied values: reporting survives pool destruction.
    { CEffect local[1]{}; manager.effects=local; manager.effect_num=1;
      local[0].active=1; ctSwitchObserveParticles(&manager, false); }
    FILE *file=std::tmpfile(); check(file); ctSwitchWriteParticleState(file); std::fclose(file);
    // A full registry evicts a record instead of growing or reading an old pointer.
    std::array<CEffectManager,70> managers{};
    for (auto &m:managers) { ++frame; ctSwitchObserveParticles(&m, true); }
    check(particle_evictions==7);
    std::puts("Particle observer: bounds, ageing, counters, pool reuse, stale storage and eviction PASS");
}
'''
cpp = out / "observer.cpp"
cpp.write_text(prefix + observer + main)
compiler = ROOT / ".kazarov/windows/deps/llvm/llvm-mingw-20260922-ucrt-x86_64/bin/clang++.exe"
exe = out / "observer.exe"
subprocess.run([str(compiler), "-std=c++23", "-O2", "-fsanitize=undefined", "-fno-sanitize-recover=all", str(cpp), "-o", str(exe)], check=True)
env = dict(os.environ, PATH=str(compiler.parent) + os.pathsep + os.environ["PATH"])
subprocess.run([str(exe)], env=env, check=True, timeout=5)
