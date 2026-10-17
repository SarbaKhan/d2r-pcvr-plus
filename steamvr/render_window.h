#pragma once
#include <windows.h>
#include <atomic>
#include "vr_resolution.h"
namespace framevr::resolution {
// Post, never SendMessage, from the render thread: the native WndProc may
// acquire renderer locks. Only the owning window thread dispatches WM_SIZE.
class RenderWindow {
    std::atomic<HWND> target{nullptr};
    std::atomic<uint64_t> desired{0},logical{0};
    std::atomic<bool> queued{false};
    std::atomic<bool> reapply{false};
public:
    static UINT Message(){static UINT id=RegisterWindowMessageW(L"D2R.PCVR.RenderResolution.v1");return id;}
    HWND Window()const{return target.load();}
    Size Logical(HWND h)const{return h==Window()?Unpack(logical.load()):Size{};}
    bool Request(HWND h,Size size,bool force=false) {
        if(!h||!IsWindow(h)||!Message())return false;
        if(target.load()!=h){target=h;logical=0;queued=false;reapply=false;}
        desired=Pack(size);
        if(!size)reapply=false;
        else if(force)reapply=true;
        if(logical.load()==desired.load()&&!reapply.load())return true;
        if(queued.exchange(true))return true;
        if(PostMessageW(h,Message(),0,0))return true;
        queued=false;return false;
    }
    void Restore(){if(auto h=Window())Request(h,{});}
    bool Handle(HWND h,UINT message,WPARAM w,LPARAM& l,WNDPROC original,LRESULT& result) {
        if(h!=Window())return false;
        if(message==Message()) {
            queued=false;
            RECT client{};
            if(!GetClientRect(h,&client)||IsIconic(h))return true;
            const auto next=desired.load();
            const bool forced=reapply.exchange(false);
            if(next==logical.load()&&!forced)return true;
            logical=next;
            const auto size=next?Unpack(next):Size{uint32_t(client.right),uint32_t(client.bottom)};
            if(size)result=CallWindowProcW(original,h,WM_SIZE,SIZE_RESTORED,MAKELPARAM(size.w,size.h));
            return true;
        }
        if(message==WM_CLOSE||message==WM_DESTROY){
            // Restore native dimensions before normal shutdown can persist a
            // windowed-size preference. Never modify Settings.json ourselves.
            desired=0;queued=false;reapply=false;
            if(logical.exchange(0)&&message==WM_CLOSE){
                RECT client{};if(GetClientRect(h,&client)&&client.right>0&&client.bottom>0)
                    CallWindowProcW(original,h,WM_SIZE,SIZE_RESTORED,MAKELPARAM(client.right,client.bottom));
            }
        }
        if(message==WM_SIZE&&w!=SIZE_MINIMIZED){
            // An actual desktop resize must not shrink the VR source again.
            // WM_SIZE carries 16-bit dimensions; Select keeps them <=4096.
            logical=desired.load();
            if(const auto size=Logical(h);size)l=MAKELPARAM(size.w,size.h);
        }
        // ScreenToClient covers polled cursor positions, but WM_MOUSE* already
        // carries client pixels and bypasses that detour. Native widget hover
        // and button dispatch must see the same render coordinates as polling.
        // Wheel/non-client messages carry screen coordinates: leave them alone.
        switch(message) {
        case WM_MOUSEMOVE:
        case WM_LBUTTONDOWN:case WM_LBUTTONUP:case WM_LBUTTONDBLCLK:
        case WM_RBUTTONDOWN:case WM_RBUTTONUP:case WM_RBUTTONDBLCLK:
        case WM_MBUTTONDOWN:case WM_MBUTTONUP:case WM_MBUTTONDBLCLK:
        case WM_XBUTTONDOWN:case WM_XBUTTONUP:case WM_XBUTTONDBLCLK:
            if(const auto size=Logical(h);size) {
                RECT client{};
                if(GetClientRect(h,&client)&&client.right>1&&client.bottom>1)
                    l=MAKELPARAM(Coordinate(short(LOWORD(l)),client.right,size.w),
                        Coordinate(short(HIWORD(l)),client.bottom,size.h));
            }
            break;
        }
        return false;
    }
};
}
