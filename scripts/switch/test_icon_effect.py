"""Exercise the actual generated sparkle function with a bounded draw recorder."""
from pathlib import Path
import argparse
import re
import subprocess
from prepare_sources import ROOT, prepare_source, parser


def function(text, name):
    masked = parser.mask(text)
    match = re.search(r'(?:static\s+)?void\s+' + name + r'\([^;{}]*\)\s*\{', masked)
    if not match:
        raise RuntimeError('Missing function: ' + name)
    depth = 1
    end = match.end()
    while depth:
        depth += (masked[end] == '{') - (masked[end] == '}')
        end += 1
    return text[match.start():end]


def main():
    args = argparse.ArgumentParser(description=__doc__)
    args.add_argument('--compiler', type=Path, default=ROOT / '.kazarov/windows/deps/llvm/llvm-mingw-20260922-ucrt-x86_64/bin/clang++.exe')
    args = args.parse_args()
    out = ROOT / 'port/build/switch-icon-effect-tests'
    out.mkdir(parents=True, exist_ok=True)
    original = ROOT / 'port/build/windows-x64-release/ps2_src/menudraw.cpp'
    before = original.read_bytes()
    generated = prepare_source(original, out / 'generated')
    text = generated.read_text(encoding='latin-1')
    code = r'''
#include <array>
#include <vector>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#define PI 3.14159265358979323846f
template<class T> struct mgRect { T left,top,right,bottom; };
struct mgCTexture {};
struct MENU_PARTS_EFFECT_STRUCT1 { float param[7]{}; };
struct MENUFORMPARTS_TYPE { MENU_PARTS_EFFECT_STRUCT1 *effect; };
enum { MENU_SPRITE_ENV_ADD, MG_PRIM_TRIANGLE_STRIP };
static void check(bool ok) { if(!ok) std::abort(); }
struct mgCDrawPrim {
    std::vector<std::array<int,2>> vertices,uv;
    int begins=0,ends=0,flushes=0;
    void Begin(int) { ++begins; }
    void End() { ++ends; }
    void Texture(mgCTexture*) {}
    void Color(int,int,int,int) {}
    void TextureCrd(int u,int v) { check(uv.size()<20); uv.push_back({u,v}); }
    void Vertex(int x,int y,int z) { check(z==0 && vertices.size()<20); vertices.push_back({x,y}); }
    void Flush() { ++flushes; }
};
static void SetSpriteEnv(mgCDrawPrim*,int) {}
static mgRect<int> star_light{0,32,8,8};
static int star_color_table[9]{255,111,111,34,230,23,0,0,128};
'''
    for name in ['ConvMGIRECTtoINTtbl', 'ConvMGFRECTtoFLOATtbl', 'DrawItemIconEffect2']:
        code += '\n' + function(text, name) + '\n'
    code += r'''
int main() {
    mgCTexture texture;
    MENU_PARTS_EFFECT_STRUCT1 effects[7]{};
    MENUFORMPARTS_TYPE part{effects};
    const mgRect<float> rect{100,200,32,32};
    for(int count=0;count<=5;++count) {
        for(int i=0;i<7;++i) effects[i]={};
        for(int i=0;i<count;++i) {
            auto &p=effects[i+2].param;
            p[0]=2; p[1]=10; p[2]=10*i; p[3]=20*i; p[5]=1;
        }
        mgCDrawPrim prim;
        DrawItemIconEffect2(&prim,&texture,&part,rect);
        check(prim.begins==1 && prim.ends==1 && prim.flushes==count);
        check(prim.vertices.size()==unsigned(count*4) && prim.uv.size()==prim.vertices.size());
        for(int i=0;i<count;++i) {
            const std::array<std::array<int,2>,4> expected{{{108+10*i,208+20*i},{100+10*i,208+20*i},{108+10*i,200+20*i},{100+10*i,200+20*i}}};
            const std::array<std::array<int,2>,4> uv{{{0,32},{8,32},{0,40},{8,40}}};
            for(int k=0;k<4;++k) {
                check(prim.vertices[i*4+k]==expected[k]);
                check(prim.uv[i*4+k]==uv[k]);
            }
        }
    }
    mgCDrawPrim empty;
    DrawItemIconEffect2(&empty,&texture,nullptr,rect);
    MENUFORMPARTS_TYPE missing{nullptr};
    DrawItemIconEffect2(&empty,&texture,&missing,rect);
    check(empty.begins==0 && empty.vertices.empty());
    std::puts("PASS: actual generated icon effect, zero through five sparkles, exact vertices/UVs, null inputs");
}
'''
    source = out / 'icon_effect.cpp'
    source.write_text(code)
    executable = out / 'icon_effect.exe'
    subprocess.run([str(args.compiler), '-std=c++17', '-O2', '-static', str(source), '-o', str(executable)], check=True)
    subprocess.run([str(executable)], check=True, timeout=5)
    assert original.read_bytes() == before, 'PC reference output changed'


if __name__ == '__main__':
    main()
