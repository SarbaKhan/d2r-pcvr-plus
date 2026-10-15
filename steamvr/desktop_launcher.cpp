// Tiny native bootstrap; private Python stays beside it, no system dependency.
#include <windows.h>
#include <string>
int WINAPI WinMain(HINSTANCE,HINSTANCE,LPSTR,int) {
    wchar_t path[32768];DWORD n=GetModuleFileNameW(nullptr,path,32768);
    if(!n||n>=32768)return 1;
    const std::wstring exe(path,n),root=exe.substr(0,exe.find_last_of(L"\\/"));
    const auto name=exe.substr(exe.find_last_of(L"\\/")+1);
    const wchar_t* mode=name.find(L"Flat")!=std::wstring::npos?L"flat":
        name.find(L"SteamVR")!=std::wstring::npos?L"steamvr":name.find(L"VDXR")!=std::wstring::npos?L"vdxr":nullptr;
    if(!mode){MessageBoxW(nullptr,L"Run D2R-PCVR-Plus-Flat.exe, SteamVR.exe or VDXR.exe.",L"D2R PCVR Plus",MB_ICONERROR);return 1;}
    const auto python=root+L"\\.runtime\\python\\python.exe";
    std::wstring command=L"\""+python+L"\" \""+root+L"\\steamvr\\direct_windows.py\" "+mode;
    STARTUPINFOW startup{};startup.cb=sizeof startup;PROCESS_INFORMATION child{};
    if(!CreateProcessW(python.c_str(),command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,root.c_str(),&startup,&child)) {
        MessageBoxW(nullptr,L"The private launcher runtime is missing or could not start. Run Setup > Update / repair.",L"D2R PCVR Plus",MB_ICONERROR);return 1;
    }
    CloseHandle(child.hThread);WaitForSingleObject(child.hProcess,INFINITE);
    DWORD code=1;GetExitCodeProcess(child.hProcess,&code);CloseHandle(child.hProcess);return int(code);
}
