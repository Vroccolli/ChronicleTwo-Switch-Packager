#include "data/files.hpp"
#include "platform/files.hpp"
#include <chrono>
#include <cstdio>
#include <fstream>
#include <functional>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace fs=std::filesystem;
namespace {
std::function<bool(const char *)> fault;
int cases=0;
void Check(bool result,const char *why) {
    ++cases;if(!result)throw std::runtime_error(std::string(why)+" (case "+std::to_string(cases)+")");
}
std::string Read(const fs::path &path) {
    char bytes[128]{};return std::string(bytes,FilesRead(path,bytes,sizeof(bytes)));
}
bool Write(const fs::path &path,std::string_view text,bool replace=false) {
    return FilesWrite(path,text.data(),text.size(),replace)==FilesResult::kWritten;
}
std::string Raw(const fs::path &path) {
    std::ifstream in(path,std::ios::binary);return {std::istreambuf_iterator<char>(in),{}};
}
void RawWrite(const fs::path &path,std::string_view bytes) {std::ofstream(path,std::ios::binary)<<bytes;}
struct Crash {};
}
bool CtSwitchFilesCheckpoint(const char *stage) {return !fault || fault(stage);}
int main() {
try {
    // Every run owns a fresh directory; fixtures are retained, never recursively
    // removed from a computed or pre-existing path.
    const auto root=fs::current_path()/("fixtures-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Check(fs::create_directory(root),"create isolated fixture root");
    auto dir=root/"saves";
    Check(FilesCreateDirectory(dir)==FilesResult::kWritten,"new directory");
    Check(FilesCreateDirectory(dir)==FilesResult::kExists,"existing directory is usable");
    const auto path=dir/"slot.json";
    Check(Write(path,"old"),"exclusive first save");
    Check(Read(path)=="old","read first save");
    Check(FilesWrite(path,"other",5,false)==FilesResult::kExists,"exclusive collision");
    Check(Read(path)=="old","collision preserves save");
    Check(Write(path,"new",true) && Read(path)=="new","overwrite existing save");
    Check(!fs::exists(path.string()+".ct-journal"),"completed journal removed");
    Check(FilesRead(dir,nullptr,0)==0,"directory is not a regular file");
    Check(FilesCreateDirectory(path)==FilesResult::kExists,"file occupies directory name");
    Check(!Write(root/"missing/slot.json","new"),"missing parent fails");
    Check(!Write(dir,"new",true) && fs::is_directory(dir),"never replace a directory");
    for(const auto *stage:{"flush-data","flush-journal","backup","publish"}) {
        Check(Write(path,"original",true),"reset failure fixture");
        fault=[&](const char *at){return std::string_view(at)!=stage;};
        Check(!Write(path,"replacement",true),"injected IO failure reported");
        fault={};Check(Read(path)=="original","IO failure preserves old data");
    }
    for(const auto *stage:{"journal-published","backup-published","primary-published"}) {
        Check(Write(path,"before",true),"reset crash fixture");
        fault=[&](const char *at){if(std::string_view(at)==stage)throw Crash{};return true;};
        bool crashed=false;try{Write(path,"after",true);}catch(const Crash &){crashed=true;}
        fault={};Check(crashed,"crash checkpoint reached");
        Check(CtSwitchFilesRecoverDirectory(dir),"recover before scanning saves");
        Check(Read(path)==(std::string_view(stage)=="primary-published"?"after":"before"),"recover intact expected generation");
        Check(!fs::exists(path.string()+".ct-journal"),"recovered journal removed");
    }
    Check(Write(path,"rollback-old",true),"reset rollback fixture");
    fault=[](const char *at){return std::string_view(at)!="publish" && std::string_view(at)!="rollback";};
    Check(!Write(path,"rollback-new",true),"rollback failure reported");
    fault={};Check(fs::exists(path.string()+".ct-journal"),"failed rollback retains journal");
    Check(Read(path)=="rollback-old","read restores previous save after failed rollback");
    auto first=dir/"first.json";
    fault=[](const char *at){if(std::string_view(at)=="journal-published")throw Crash{};return true;};
    try{Write(first,"first-complete",true);}catch(const Crash &){}
    fault={};Check(CtSwitchFilesRecoverDirectory(dir) && Read(first)=="first-complete","recover complete pending first save");
    Check(Write(path,"before-cleanup",true),"reset cleanup fixture");
    fault=[](const char *at){return std::string_view(at)!="cleanup";};
    Check(Write(path,"after-cleanup",true),"published save succeeds despite delayed cleanup");
    fault={};Check(Read(path)=="after-cleanup","recover completed publication without rollback");
    // A malformed journal is never permission to remove or replace user data.
    auto broken=dir/"broken.json";RawWrite(broken,"keep");RawWrite(broken.string()+".ct-journal","not ours\n");
    Check(!Write(broken,"overwrite",true),"malformed journal refuses writes");
    Check(Raw(broken)=="keep" && Raw(broken.string()+".ct-journal")=="not ours\n","malformed journal preserves all bytes");
    // Journal names cannot escape the target's parent directory.
    auto escape=dir/"escape.json";RawWrite(escape,"keep");RawWrite(escape.string()+".ct-journal",
        "ChronicleTwo-Horizon-publication-v1\n../outside.ct-0000000000000000.tmp\n../outside.ct-0000000000000000.bak\n");
    Check(!CtSwitchFilesRecover(escape) && Raw(escape)=="keep","reject journal traversal");
    // Competing first saves must publish exactly one complete document.
    auto race=dir/"race.json";std::vector<FilesResult> results(8);std::vector<std::thread> threads;
    for(int i=0;i<8;++i)threads.emplace_back([&,i]{auto value="writer-"+std::to_string(i);results[i]=FilesWrite(race,value.data(),value.size(),false);});
    for(auto &thread:threads)thread.join();
    int winners=0;for(auto result:results)winners+=result==FilesResult::kWritten;
    Check(winners==1 && Read(race).starts_with("writer-"),"exclusive concurrent publication");
    std::printf("%d Horizon publication/recovery checks passed\nFixtures: %s\n",cases,root.string().c_str());
    return 0;
}catch(const std::exception &error){std::fprintf(stderr,"%s\n",error.what());return 1;}
}
