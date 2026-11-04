#include "gfx/gfx.hpp"
#include <SDL3/SDL.h>
#include <array>
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string_view>
#include <vector>
extern "C" void ctRendererProbeStart();
extern "C" void ctRendererProbeGraphics();
extern "C" int ctRendererProbeExit();
namespace {
using Color = std::array<uint8_t,4>;
void AddQuad(std::vector<gfx::Vertex2D> &v,float x,float y,float w,float h,Color color) {
    for (auto corner : std::array<std::array<float,4>,4>{{{x,y,0,0},{x+w,y,2,0},{x+w,y+h,2,2},{x,y+h,0,2}}}) {
        gfx::Vertex2D vertex{}; vertex.x=corner[0]; vertex.y=corner[1]; vertex.u=corner[2]; vertex.v=corner[3];
        std::copy(color.begin(),color.end(),vertex.color); v.push_back(vertex);
    }
}
void Quad(float x,float y,float w,float h,Color color,gfx::TextureBinding texture={},gfx::DrawState state={}) {
    std::vector<gfx::Vertex2D> vertices; AddQuad(vertices,x,y,w,h,color);
    gfx::Draw2D(gfx::Primitive::Quads,vertices,texture,state);
}
void Text(std::string_view text,float x,float y,Color color) {
    // A compact diagnostic font drawn through the real game's 2D renderer.
    static constexpr uint8_t glyphs[26][7] = {
        {14,17,17,31,17,17,17},{30,17,17,30,17,17,30},{15,16,16,16,16,16,15},
        {30,17,17,17,17,17,30},{31,16,16,30,16,16,31},{31,16,16,30,16,16,16},
        {15,16,16,23,17,17,15},{17,17,17,31,17,17,17},{14,4,4,4,4,4,14},
        {7,2,2,2,18,18,12},{17,18,20,24,20,18,17},{16,16,16,16,16,16,31},
        {17,27,21,21,17,17,17},{17,25,21,19,17,17,17},{14,17,17,17,17,17,14},
        {30,17,17,30,16,16,16},{14,17,17,17,21,18,13},{30,17,17,30,20,18,17},
        {15,16,16,14,1,1,30},{31,4,4,4,4,4,4},{17,17,17,17,17,17,14},
        {17,17,17,17,17,10,4},{17,17,17,21,21,27,17},{17,17,10,4,10,17,17},
        {17,17,10,4,4,4,4},{31,1,2,4,8,16,31}};
    std::vector<gfx::Vertex2D> vertices;
    for (char c : text) {
        if (c>='A' && c<='Z') for (int row=0;row<7;++row) for (int col=0;col<5;++col)
            if (glyphs[c-'A'][row]&(16>>col)) AddQuad(vertices,x+col*3,y+row*3,3,3,color);
        x+=18;
    }
    if (!vertices.empty()) gfx::Draw2D(gfx::Primitive::Quads,vertices,{},{});
}
bool Pixel(const std::vector<uint8_t> &image,uint32_t width,uint32_t x,uint32_t y,Color expected,int tolerance=1) {
    auto offset=(static_cast<size_t>(y)*width+x)*4;
    if (offset+4>image.size()) return false;
    for (int c=0;c<3;++c) if (std::abs(int(image[offset+c])-int(expected[c]))>tolerance) return false;
    return true;
}
}
int main() {
    ctRendererProbeStart();
    const std::filesystem::path root="sdmc:/switch/ChronicleTwo-PC-Switch/renderer-probe";
    std::error_code error; std::filesystem::create_directories(root,error);
    std::freopen((root/"stdout.log").c_str(),"wb",stdout);
    std::freopen((root/"stderr.log").c_str(),"wb",stderr);
    std::setvbuf(stdout,nullptr,_IONBF,0); std::setvbuf(stderr,nullptr,_IONBF,0);
    std::puts("Renderer probe 0.6.0: initializing SDL3");
    if (!SDL_Init(SDL_INIT_VIDEO|SDL_INIT_GAMEPAD)) { std::fprintf(stderr,"SDL_Init: %s\n",SDL_GetError()); return 1; }
    SDL_Window *window=SDL_CreateWindow("ChronicleTwo renderer probe",1280,720,SDL_WINDOW_VULKAN|SDL_WINDOW_FULLSCREEN);
    if (!window) { std::fprintf(stderr,"SDL_CreateWindow: %s\n",SDL_GetError()); return 1; }
    ctRendererProbeGraphics();
    gfx::RendererConfig config; config.app_name="ChronicleTwo renderer probe";
    config.logical_width=640; config.logical_height=480; config.render_scale=1;
    config.validation=false; config.prog0_meshes=true;
    config.pipeline_cache=root/"pipeline-cache.bin";
    config.progress=[](uint32_t done,uint32_t total) { if (done%25==0 || done==total) std::printf("pipelines %u/%u\n",done,total); };
    std::puts("Creating the unchanged engine Vulkan renderer");
    gfx::RendererInit(window,config);
    std::puts("Renderer initialized; testing real GPU pixels");
    auto target=gfx::CreateRenderTarget(64,64,true);
    auto rgba=gfx::CreateTexture({.width=2,.height=2});
    auto indexed=gfx::CreateTexture({.width=2,.height=2,.format=gfx::TextureFormat::Index8});
    auto palette=gfx::CreatePalette();
    std::array<uint32_t,4> colors={0x800000ff,0x8000ff00,0x80ff0000,0x80ffffff};
    std::array<uint8_t,4> indices={1,1,1,1};
    bool upload=gfx::UpdateTexture(rgba,0,0,0,2,2,colors.data()) &&
        gfx::UpdateTexture(indexed,0,0,0,2,2,indices.data()) && gfx::UpdatePalette(palette,colors.data(),0,4);
    Color red={255,0,0,128};
    bool began=gfx::BeginFrame();
    if (began) {
        gfx::SetRenderTarget(target); gfx::Clear(true,red.data(),true,0);
        Quad(8,8,16,16,{0,255,0,128});
        Quad(32,8,16,16,{128,128,128,128},{.texture=indexed,.palette=palette,.filter=gfx::Filter::Nearest});
        gfx::DrawState blend; blend.blend=true;
        Quad(8,32,16,16,{0,0,255,64},{},blend);
        gfx::SetRenderTarget(gfx::kMainTarget); gfx::EndFrame();
    }
    std::vector<uint8_t> pixels,uploaded; uint32_t w=0,h=0,tw=0,th=0;
    bool read=gfx::ReadbackTexture(target,pixels,w,h) && w==64 && h==64;
    bool textureRead=gfx::ReadbackTexture(rgba,uploaded,tw,th) && tw==2 && th==2 && uploaded.size()==16;
    if (textureRead) textureRead=std::memcmp(uploaded.data(),colors.data(),16)==0;
    std::array<bool,4> pass={began && read && Pixel(pixels,w,1,1,red) && Pixel(pixels,w,16,16,{0,255,0,128}),
        upload && textureRead, read && Pixel(pixels,w,40,16,{0,255,0,128}), read && Pixel(pixels,w,16,40,{127,0,128,128},3)};
    const std::array<const char *,4> labels={"CLEAR AND DRAW","RGBA TEXTURE","INDEXED TEXTURE","GS BLEND"};
    int failures=0; std::ofstream result(root/"result.txt"); result<<"Renderer probe 0.6.0\n";
    for (size_t i=0;i<pass.size();++i) { result<<labels[i]<<": "<<(pass[i]?"PASS":"FAIL")<<'\n'; if (!pass[i]) failures|=1<<i; }
    result<<"Failure bits: "<<failures<<'\n'; result.close();
    std::printf("GPU tests completed: failure bits %d\n",failures);
    bool saved=false;
    while (!ctRendererProbeExit()) {
        SDL_Event event; while (SDL_PollEvent(&event)) if (event.type==SDL_EVENT_QUIT) goto done;
        if (!gfx::BeginFrame()) continue;
        Color background={8,12,24,128}; gfx::Clear(true,background.data(),true,0);
        Text("CT RENDER",32,32,{255,255,255,128});
        for (size_t i=0;i<pass.size();++i) {
            Color color=pass[i]?Color{40,255,100,128}:Color{255,60,60,128};
            Text(labels[i],32,100+i*65,{210,220,240,128}); Text(pass[i]?"PASS":"FAIL",430,100+i*65,color);
        }
        Text("PRESS PLUS TO EXIT",32,410,{160,180,210,128});
        gfx::EndFrame();
        if (!saved) {
            std::vector<uint8_t> screen; uint32_t sw=0,sh=0;
            if (gfx::ReadbackFrame(screen,sw,sh)) gfx::WritePng(root/"frame.png",screen.data(),sw,sh);
            saved=true;
        }
    }
done:
    gfx::DestroyTexture(palette); gfx::DestroyTexture(indexed); gfx::DestroyTexture(rgba); gfx::DestroyTexture(target);
    gfx::RendererShutdown(); SDL_DestroyWindow(window); SDL_Quit();
    return failures != 0;
}
