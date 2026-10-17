#include "logging.h"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

namespace framevr::logging {
namespace {
// Bounded queue: game/render callbacks never perform file IO, wait for a
// writer, or force durable filesystem commits. Diagnostics may be dropped
// under extreme pressure; gameplay must keep moving.
SRWLOCK lock=SRWLOCK_INIT;
struct Row {char text[640];};
std::array<Row,1024> rows;
uint32_t count=0,dropped=0;
bool accepting=false;
HANDLE file=INVALID_HANDLE_VALUE,stop=nullptr,worker=nullptr;
DWORD WINAPI Writer(void*) {
    std::string batch;batch.reserve(640*1024);
    for(;;){
        const bool ending=WaitForSingleObject(stop,250)==WAIT_OBJECT_0;
        batch.clear();
        AcquireSRWLockExclusive(&lock);
        for(uint32_t i=0;i<count;++i)batch+=rows[i].text;
        if(dropped){char line[100];std::snprintf(line,sizeof line,"Frame logger: dropped %u diagnostic records (queue full)\n",dropped);batch+=line;}
        count=dropped=0;
        ReleaseSRWLockExclusive(&lock);
        size_t at=0;
        while(at<batch.size()){
            DWORD written=0;
            if(!WriteFile(file,batch.data()+at,DWORD(batch.size()-at),&written,nullptr)||!written)break;
            at+=written;
        }
        // Deliberately no FlushFileBuffers: the SDK's per-message fsync was
        // observed blocking the game's main thread for hundreds of ms.
        if(ending)break;
    }
    return 0;
}
}
bool Start(HMODULE plugin){
    wchar_t path[MAX_PATH];const DWORD len=GetModuleFileNameW(plugin,path,MAX_PATH);
    if(!len||len>=MAX_PATH)return false;
    std::wstring p(path);const auto last=p.find_last_of(L"\\/");if(last==p.npos)return false;
    p.resize(last);const auto parent=p.find_last_of(L"\\/");if(parent==p.npos)return false;
    p.resize(parent+1);p+=L"logs";
    // A fresh mod-folder install has no log directory until its first launch.
    if(!CreateDirectoryW(p.c_str(),nullptr)&&GetLastError()!=ERROR_ALREADY_EXISTS)return false;
    p+=L"\\d2r-vr-frame.log";
    file=CreateFileW(p.c_str(),FILE_APPEND_DATA,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE)return false;
    stop=CreateEventW(nullptr,TRUE,FALSE,nullptr);
    if(stop)worker=CreateThread(nullptr,0,&Writer,nullptr,0,nullptr);
    if(!worker){if(stop)CloseHandle(stop);CloseHandle(file);stop=nullptr;file=INVALID_HANDLE_VALUE;return false;}
    AcquireSRWLockExclusive(&lock);accepting=true;ReleaseSRWLockExclusive(&lock);
    Write("Frame logger: buffered diagnostics started; routine logging does not force disk commits");return true;
}
void Write(const char* text){
    if(!text)return;
    SYSTEMTIME t{};GetLocalTime(&t);
    // The short queue lock cannot be held across disk IO by the writer.
    AcquireSRWLockExclusive(&lock);
    if(accepting){
        if(count<rows.size())std::snprintf(rows[count++].text,sizeof rows[0].text,"[%04u-%02u-%02u %02u:%02u:%02u.%03u] [INFO] [Frame] %s\n",t.wYear,t.wMonth,t.wDay,t.wHour,t.wMinute,t.wSecond,t.wMilliseconds,text);
        else ++dropped;
    }
    ReleaseSRWLockExclusive(&lock);
}
void Stop(){
    AcquireSRWLockExclusive(&lock);accepting=false;ReleaseSRWLockExclusive(&lock);
    if(worker){SetEvent(stop);WaitForSingleObject(worker,INFINITE);CloseHandle(worker);worker=nullptr;}
    if(stop){CloseHandle(stop);stop=nullptr;}
    if(file!=INVALID_HANDLE_VALUE){CloseHandle(file);file=INVALID_HANDLE_VALUE;}
}
}
