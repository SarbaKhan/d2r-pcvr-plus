#include "logging.h"
#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>

DWORD WINAPI Produce(void* id){
    for(int i=0;i<200;++i){char line[60];std::snprintf(line,sizeof line,"producer-%llu-record-%03d",(unsigned long long)id,i);framevr::logging::Write(line);}
    return 0;
}
int main(){
    // CMake places this executable in its own logging-fixture/plugins folder.
    // Reset only that fixture's output so repeated runs check a fresh session.
    wchar_t path[MAX_PATH];GetModuleFileNameW(nullptr,path,MAX_PATH);std::wstring p(path);
    p.resize(p.find_last_of(L"\\/"));p.resize(p.find_last_of(L"\\/")+1);p+=L"logs\\d2r-vr-frame.log";
    if(!DeleteFileW(p.c_str())&&GetLastError()!=ERROR_FILE_NOT_FOUND&&GetLastError()!=ERROR_PATH_NOT_FOUND)return 6;
    const auto folder=p.substr(0,p.find_last_of(L"\\/"));
    if(!RemoveDirectoryW(folder.c_str())&&GetLastError()!=ERROR_PATH_NOT_FOUND&&GetLastError()!=ERROR_FILE_NOT_FOUND)return 7;
    if(!framevr::logging::Start(nullptr))return 1;
    HANDLE threads[]={CreateThread(nullptr,0,Produce,(void*)1,0,nullptr),CreateThread(nullptr,0,Produce,(void*)2,0,nullptr)};
    if(!threads[0]||!threads[1])return 2;
    WaitForMultipleObjects(2,threads,TRUE,INFINITE);for(auto t:threads)CloseHandle(t);
    framevr::logging::Stop();framevr::logging::Write("after-stop-must-not-appear");
    FILE* file=_wfopen(p.c_str(),L"rb");if(!file)return 3;
    std::string text;char buf[4096];size_t n;while((n=fread(buf,1,sizeof buf,file)))text.append(buf,n);fclose(file);
    if(text.find("after-stop-must-not-appear")!=text.npos)return 4;
    for(int producer=1;producer<=2;++producer){
        size_t previous=0;
        for(int i=0;i<200;++i){char line[60];std::snprintf(line,sizeof line,"producer-%d-record-%03d",producer,i);auto at=text.find(line);if(at==text.npos||(i&&at<=previous)||text.find(line,at+1)!=text.npos)return 5;previous=at;}
    }
    puts("Buffered logger: both producers preserved in order; shutdown drains pending records");return 0;
}
