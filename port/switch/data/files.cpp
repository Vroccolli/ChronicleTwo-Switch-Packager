#include "platform/files.hpp"
#include "data/files.hpp"

#include <array>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#include <io.h>
#else
#include <unistd.h>
#endif
#ifndef O_CLOEXEC
#define O_CLOEXEC 0
#endif
#ifndef O_NONBLOCK
#define O_NONBLOCK 0
#endif
#ifndef O_BINARY
#define O_BINARY 0
#endif

// Horizon has exclusive namespace renames, file flush, and synchronous native
// metadata operations. It has neither POSIX directory descriptors/fsync nor
// hard links nor rename-over-existing. A flushed journal and an old-file backup
// bridge replacement's two renames; recovery runs before reads and save scans.
namespace fs=std::filesystem;
#ifdef CT_SWITCH_FILES_TESTING
extern bool CtSwitchFilesCheckpoint(const char *stage);
#endif
namespace {
std::mutex mutex;
constexpr std::string_view journal_suffix=".ct-journal";
constexpr std::string_view magic="ChronicleTwo-Horizon-publication-v1";

bool Point(const char *stage) {
#ifdef CT_SWITCH_FILES_TESTING
    if(!CtSwitchFilesCheckpoint(stage)) {errno=EIO;return false;}
#endif
    return true;
}
enum class Kind { Missing, Regular, Other, Error };
Kind Type(const fs::path &path) {
    std::error_code error;
    auto status=fs::symlink_status(path,error);
    if(error==std::errc::no_such_file_or_directory || status.type()==fs::file_type::not_found) return Kind::Missing;
    if(error) return Kind::Error;
    return fs::is_regular_file(status)?Kind::Regular:Kind::Other;
}
void RemoveOwned(const fs::path &path) {std::remove(path.string().c_str());}
bool RenameExclusive(const fs::path &from,const fs::path &to) {
    // Both Horizon and the Windows CRT used by the host model refuse occupied
    // destinations. Do not substitute std::filesystem::rename, which replaces
    // on desktop platforms.
    return std::rename(from.string().c_str(),to.string().c_str())==0;
}
int Open(const fs::path &path,int flags) {
    return open(path.string().c_str(),flags|O_BINARY|O_CLOEXEC,0644);
}
bool Close(int fd) {return close(fd)==0;}
bool Flush(int fd) {
    int result;
    do {
#ifdef _WIN32
        result=_commit(fd);
#else
        result=fsync(fd);
#endif
    }while(result!=0 && errno==EINTR);
    return result==0;
}
bool WriteAll(int fd,const char *data,std::size_t size) {
    while(size) {
        const auto chunk=static_cast<unsigned>(size>0x40000000?0x40000000:size);
        const auto written=write(fd,data,chunk);
        if(written<0 && errno==EINTR)continue;
        if(written<=0)return false;
        data+=written;size-=static_cast<std::size_t>(written);
    }
    return true;
}
fs::path Journal(const fs::path &path) {return fs::path(path.string()+std::string(journal_suffix));}
fs::path NewTemp(const fs::path &path) {
    static std::uint64_t serial=static_cast<std::uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count());
    std::array<char,32> suffix{};
    std::snprintf(suffix.data(),suffix.size(),".ct-%016llx.tmp",static_cast<unsigned long long>(++serial));
    return fs::path(path.string()+suffix.data());
}
bool ValidLeaf(const fs::path &path,const std::string &leaf,std::string_view suffix) {
    const std::string prefix=path.filename().string()+".ct-";
    return leaf.size()==prefix.size()+16+suffix.size() && leaf.starts_with(prefix) && leaf.ends_with(suffix) &&
        leaf.substr(prefix.size(),16).find_first_not_of("0123456789abcdef")==std::string::npos &&
        leaf.find_first_of("/\\\r\n")==std::string::npos;
}
struct Publication {fs::path temp,backup;};
bool ReadJournal(const fs::path &path,Publication &p) {
    const auto journal=Journal(path);
    if(Type(journal)!=Kind::Regular)return false;
    std::error_code error;
    if(fs::file_size(journal,error)>2048 || error)return false;
    std::ifstream input(journal,std::ios::binary);
    std::string header,temp,backup,extra;
    if(!std::getline(input,header) || !std::getline(input,temp) || !std::getline(input,backup) ||
        std::getline(input,extra) || !input.eof() || header!=magic ||
        !ValidLeaf(path,temp,".tmp") || !ValidLeaf(path,backup,".bak") ||
        backup!=temp.substr(0,temp.size()-4)+".bak")return false;
    p={path.parent_path()/temp,path.parent_path()/backup};return true;
}
bool Recover(const fs::path &path) {
    const auto journal=Journal(path);
    const auto journal_kind=Type(journal);
    if(journal_kind==Kind::Missing)return true;
    if(journal_kind!=Kind::Regular)return false;
    Publication p;
    if(!ReadJournal(path,p))return false;
    const auto primary=Type(path),backup=Type(p.backup),temp=Type(p.temp);
    if(primary==Kind::Other || primary==Kind::Error || backup==Kind::Other || backup==Kind::Error ||
        temp==Kind::Other || temp==Kind::Error)return false;
    if(primary==Kind::Missing) {
        // Restore the previous generation preferentially. With no previous
        // file, a complete flushed temporary may finish its pending creation.
        const auto &from=backup==Kind::Regular?p.backup:p.temp;
        if(Type(from)!=Kind::Regular || !RenameExclusive(from,path))return false;
    } else if(backup==Kind::Regular && temp==Kind::Regular) {
        // Another writer occupied the primary before publication. Keep all
        // three generations and refuse recovery rather than deleting progress.
        return false;
    }
    RemoveOwned(p.temp);RemoveOwned(p.backup);RemoveOwned(journal);
    return Type(journal)==Kind::Missing;
}
bool RecoverDirectory(const fs::path &directory) {
    std::error_code error;
    fs::directory_iterator iter(directory,error),end;
    if(error==std::errc::no_such_file_or_directory)return true;
    if(error)return false;
    std::vector<fs::path> paths;
    for(;iter!=end;iter.increment(error)) {
        if(error)return false;
        const auto name=iter->path().filename().string();
        if(name.ends_with(journal_suffix))paths.push_back(fs::path(iter->path().string().substr(0,iter->path().string().size()-journal_suffix.size())));
    }
    if(error)return false;
    for(const auto &path:paths)if(!Recover(path))return false;
    return true;
}
bool WriteFlushed(int fd,const char *data,std::size_t size,const char *checkpoint) {
    const bool written=WriteAll(fd,data,size);
    const bool flushed=written && Point(checkpoint) && Flush(fd);
    return Close(fd) && flushed;
}
FilesResult Publish(const fs::path &temp,const fs::path &path,bool replace) {
    if(!replace) {
        if(Point("publish") && RenameExclusive(temp,path))return FilesResult::kWritten;
        return Type(path)!=Kind::Missing && Type(path)!=Kind::Error?FilesResult::kExists:FilesResult::kFailed;
    }
    if(Type(path)==Kind::Other || Type(path)==Kind::Error)return FilesResult::kFailed;
    const fs::path backup=temp.string().substr(0,temp.string().size()-4)+".bak";
    if(Type(backup)!=Kind::Missing)return FilesResult::kFailed;
    const auto journal=Journal(path),journal_temp=fs::path(temp.string()+".journal");
    const std::string text=std::string(magic)+"\n"+temp.filename().string()+"\n"+backup.filename().string()+"\n";
    int fd=Open(journal_temp,O_WRONLY|O_CREAT|O_EXCL);
    if(fd<0)return FilesResult::kFailed;
    if(!WriteFlushed(fd,text.data(),text.size(),"flush-journal") || !RenameExclusive(journal_temp,journal)) {
        RemoveOwned(journal_temp);return FilesResult::kFailed;
    }
    Point("journal-published");
    const bool existed=Type(path)==Kind::Regular;
    if(existed && (!Point("backup") || !RenameExclusive(path,backup))) {
        RemoveOwned(journal);return FilesResult::kFailed;
    }
    Point("backup-published");
    if(!Point("publish") || !RenameExclusive(temp,path)) {
        if(!existed || (Point("rollback") && RenameExclusive(backup,path)))RemoveOwned(journal);
        // If rollback fails, keep both data generations and the journal. The
        // caller must not clean the temp referenced by this recovery record.
        return FilesResult::kFailed;
    }
    Point("primary-published");
    if(Point("cleanup")) {RemoveOwned(backup);RemoveOwned(journal);}
    return FilesResult::kWritten;
}
}

bool CtSwitchFilesRecover(const fs::path &path) {
    std::lock_guard lock(mutex);return Recover(path);
}
bool CtSwitchFilesRecoverDirectory(const fs::path &path) {
    std::lock_guard lock(mutex);return RecoverDirectory(path);
}
FilesResult FilesWrite(const fs::path &path,const void *data,std::size_t size,bool replace) {
    std::lock_guard lock(mutex);
    if(!Recover(path))return FilesResult::kFailed;
    if(path.filename().string().find_first_of("\r\n")==std::string::npos) {
        fs::path temp;int fd=-1;
        for(int attempt=0;attempt<8 && fd<0;++attempt) {
            temp=NewTemp(path);fd=Open(temp,O_WRONLY|O_CREAT|O_EXCL);
            if(fd<0 && errno!=EEXIST)return FilesResult::kFailed;
        }
        if(fd<0)return FilesResult::kFailed;
        if(!WriteFlushed(fd,static_cast<const char *>(data),size,"flush-data")) {
            RemoveOwned(temp);return FilesResult::kFailed;
        }
        const auto result=Publish(temp,path,replace);
        if(result!=FilesResult::kWritten && Type(Journal(path))==Kind::Missing)RemoveOwned(temp);
        return result;
    }
    return FilesResult::kFailed;
}
std::size_t FilesRead(const fs::path &path,void *data,std::size_t size) {
    std::lock_guard lock(mutex);
    if(!Recover(path) || Type(path)!=Kind::Regular)return 0;
    int fd=Open(path,O_RDONLY|O_NONBLOCK);
    if(fd<0)return 0;
    std::size_t total=0;
    while(total<size) {
        const auto chunk=static_cast<unsigned>(size-total>0x40000000?0x40000000:size-total);
        const auto count=read(fd,static_cast<char *>(data)+total,chunk);
        if(count<0 && errno==EINTR)continue;
        if(count<=0)break;
        total+=static_cast<std::size_t>(count);
    }
    Close(fd);return total;
}
FilesResult FilesCreateDirectory(const fs::path &path) {
    std::lock_guard lock(mutex);
#ifdef _WIN32
    const int result=mkdir(path.string().c_str());
#else
    const int result=mkdir(path.string().c_str(),0755);
#endif
    if(result!=0 && errno!=EEXIST)return FilesResult::kFailed;
    std::error_code error;
    if(fs::is_directory(path,error) && !RecoverDirectory(path))return FilesResult::kFailed;
    return result==0?FilesResult::kWritten:FilesResult::kExists;
}
