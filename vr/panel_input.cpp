#include <windows.h>
#include <atomic>
#include <cstring>
#include "panel_input.h"
#include "panel_input_mode.h"
#include "sigscan.h"
#include "logging.h"

namespace panelinput {
namespace {
framevr::panels::InputModeLease lease;
framevr::panels::WorldInputMode worldInput;
using Activity=bool(*)(void*);
using Request=bool(*)(int);
using Controller=bool(*)();
Activity originalActivity=nullptr;
Request originalRequest=nullptr;
Controller isController=nullptr;
Controller originalCursorSnap=nullptr;
using Dispatch=void(*)(const void*);
using Focus=void(*)(void*,bool);
Dispatch originalDispatch=nullptr;
Focus originalFocus=nullptr;
Activity originalPointer[6]{};
bool pointerIn[6]{},activityIn=false,requestIn=false,controllerIn=false,dispatchIn=false,focusIn=false,cursorSnapIn=false;
std::atomic<bool> ready{false},reported{false};
std::atomic<bool> pointerFocus{false};
thread_local bool pointerContext=false;
thread_local bool bindingContext=false;

// 3.3.93787: the input-event preflight requests KB/M then clears every pad's
// state. Returning false means "no layout transition pending", so dispatch
// continues to the existing widgets. Do not return true (consumes the event),
// or call the preflight and merely suppress its mode setter (still clears pads).
constexpr uint64_t ActivityRva=0x13b480;
constexpr uint8_t ActivitySig[]={0x48,0x83,0xec,0x48,0xe8,0xf7,0xc8,0xf3,0xff,0x84,0xc0,0x0f,0x85,0xc2,0x01,0x00,0x00,
    0xe8,0x7a,0xc9,0xf3,0xff,0x84,0xc0,0x0f,0x84,0xb5,0x01,0x00,0x00};
constexpr uint64_t ActivityRequestRva=0x13b5ea;
constexpr uint8_t ActivityRequestSig[]={0x33,0xc9,0xe8,0x3f,0xc0,0x70,0x00,0x48,0x8d,0xbb,0x40,0x0e,0x00,0x00,0x0f,0xb6,0xf0};
// The shared mode request forwards (manager,method) to the native transition;
// method=0 is KB/M, method=1 controller. Keep real controller requests intact.
constexpr uint64_t RequestRva=0x847630;
constexpr uint8_t RequestSig[]={0x8b,0xd1,0x48,0x8b,0x0d,0x37,0x8b,0xbf,0x02,0x48,0x85,0xc9,0x0f,0x85,0xfe,0xdd,0x05,0x00,0x32,0xc0,0xc3};
constexpr uint64_t ControllerRva=0x77e10;
constexpr uint8_t ControllerSig[]={0x48,0x83,0xec,0x28,0xe8,0xf7,0xe1,0xff,0xff,0x80,0xb8,0xc8,0x0d,0x00,0x00,0x00,
    0x75,0x10,0x80,0x3d,0x4b,0xb4,0x9a,0x02,0x00};
constexpr uint64_t DispatchRva=0x843d90;
constexpr uint8_t DispatchSig[]={0x40,0x53,0x56,0x57,0x48,0x83,0xec,0x20,0x4c,0x89,0x7c,0x24,0x58,0x4c,0x8b,0xf9};
constexpr uint64_t MessageLayoutRva=0x866fbd;
constexpr uint8_t MessageLayoutSig[]={0x48,0x8b,0x86,0x88,0x00,0x00,0x00,0x48,0xb9,0x8c,0x93,0x7c,0xac,0x2c,0x17,0xd5,0x02};
constexpr uint64_t FocusRva=0x8676e0;
constexpr uint8_t FocusSig[]={0x40,0x55,0x41,0x57,0x48,0x8d,0xac,0x24,0x38,0xfd,0xff,0xff,0x48,0x81,0xec,0xc8,0x03,0x00,0x00};
// FocusManager calls this after dispatching MouseMove. In controller layout
// it replaces the exact hovered widget with the closest controller cursor
// region (867bd0..867ce3), stealing Yes/No focus from NpcDialogPanel. This is
// NOT the controller-layout query above. Never change its native global flag.
constexpr uint64_t CursorSnapRva=0x14f1d0;
constexpr uint8_t CursorSnapSig[]={0x0f,0xb6,0x05,0x25,0x95,0x93,0x02,0xc3};
constexpr uint64_t FocusSnapRva=0x867bd0;
constexpr uint8_t FocusSnapSig[]={0xe8,0xfb,0x75,0x8e,0xff,0x84,0xc0,0x0f,0x84,0xac,0x01,0x00,0x00};
// Native UISystem event callbacks: move, left down/up, right down/up, scroll.
// All accept the native event pointer and return whether it was consumed.
constexpr uint64_t PointerRvas[]={0x83f340,0x83f400,0x83f870,0x83fcc0,0x840120,0x840dc0};
constexpr uint8_t MoveSig[]={0x40,0x57,0x48,0x83,0xec,0x20,0x48,0x8b,0xf9,0xe8,0x32,0x8a,0x83,0xff};
constexpr uint8_t ButtonSig[]={0x48,0x89,0x74,0x24,0x20,0x55,0x48,0x8d,0xac,0x24,0xe0,0xfd,0xff,0xff,0x48,0x81,0xec,0x20,0x03,0x00,0x00};

bool Owns(){return ready.load(std::memory_order_acquire)&&lease.Active(GetTickCount64());}
bool ControllerMode(){return bindingContext?true:pointerContext?false:isController();}
bool CursorSnap(){return framevr::panels::CursorSnapAllowed(pointerContext,bindingContext)&&originalCursorSnap();}
template<int I> bool OnPointer(void* event) {
    return framevr::panels::RoutePointer(Owns(),pointerContext,[&]{return originalPointer[I](event);});
}
constexpr Activity pointerHooks[]={&OnPointer<0>,&OnPointer<1>,&OnPointer<2>,&OnPointer<3>,&OnPointer<4>,&OnPointer<5>};
void OnDispatch(const void* message) {
    uint64_t target=0,command=0;
    if(message){std::memcpy(&target,message,8);std::memcpy(&command,static_cast<const char*>(message)+0x88,8);}
    framevr::panels::PointerScope scope(pointerContext,Owns()&&framevr::panels::PointerMessage(target,command));
    originalDispatch(message);
}
void OnFocus(void* self,bool sound) {
    framevr::panels::PointerScope scope(pointerContext,framevr::panels::FocusUsesPointer(Owns(),pointerFocus.load()));
    originalFocus(self,sound);
}

bool Preserve() {
    if(!ready.load(std::memory_order_acquire)||!lease.PreserveController(GetTickCount64(),true))return false;
    return isController();
}
void Report() {
    if(!reported.exchange(true))framevr::logging::Write("Frame panels: layout switch suppressed; laser routed through UI-only pointer dispatch");
}
bool OnActivity(void* event) {
    if(Preserve()){Report();return false;}
    return originalActivity(event);
}
bool OnRequest(int method) {
    if(method==0&&Preserve()){Report();return false;}
    return originalRequest(method);
}
}
void OwnPointer(){pointerFocus=true;lease.Refresh(GetTickCount64());}
void UseController(){pointerFocus=false;}
void WorldInput(bool active){worldInput.Update(GetTickCount64(),active);}
void MaintainWorldController(bool player,bool menu){
    if(!ready.load(std::memory_order_acquire))return;
    if(worldInput.NeedsController(GetTickCount64(),player,menu,isController())) {
        // Use the same verified native transition as the tabbed menu. A fake
        // A press would attack when no target exists and cannot solve the
        // circular dependency between acquiring a target and accepting A.
        static uint64_t reportAt=0;
        const auto now=GetTickCount64();
        if(now>=reportAt){framevr::logging::Write("Frame input: restoring native controller layout for VR world interactions");reportAt=now+2000;}
        originalRequest(1);
    }
}
bool BlocksWorld(){return Owns();}
bool ControllerAction(void (*action)(void*),void* data){
    if(!ready.load(std::memory_order_acquire)||!action)return false;
    const bool previous=bindingContext;bindingContext=true;
    action(data);bindingContext=previous;return true;
}
bool ControllerReady(){
    if(!ready.load(std::memory_order_acquire))return false;
    if(isController())return true;
    originalRequest(1); // native transition; its UI update commits/reloads layouts
    return false;
}
void Install() {
    if(ready.load())return;
    if((!controllerIn&&!d2rsig::Check(ControllerRva,ControllerSig,sizeof ControllerSig))||
       !d2rsig::Check(ActivityRequestRva,ActivityRequestSig,sizeof ActivityRequestSig))return;
    // Validate every entry point before changing any of them. Partial installs
    // pass through until all callbacks and the layout guard are ready.
    if((!activityIn&&!d2rsig::Check(ActivityRva,ActivitySig,sizeof ActivitySig))||
       (!requestIn&&!d2rsig::Check(RequestRva,RequestSig,sizeof RequestSig))||
       (!dispatchIn&&!d2rsig::Check(DispatchRva,DispatchSig,sizeof DispatchSig))||
       (!focusIn&&!d2rsig::Check(FocusRva,FocusSig,sizeof FocusSig))||
       (!cursorSnapIn&&!d2rsig::Check(CursorSnapRva,CursorSnapSig,sizeof CursorSnapSig))||
       !d2rsig::Check(FocusSnapRva,FocusSnapSig,sizeof FocusSnapSig)||
       !d2rsig::Check(MessageLayoutRva,MessageLayoutSig,sizeof MessageLayoutSig))return;
    for(int i=0;i<6;++i)if(!pointerIn[i]&&!d2rsig::Check(PointerRvas[i],i?ButtonSig:MoveSig,i?sizeof ButtonSig:sizeof MoveSig))return;
    if(!controllerIn)controllerIn=d2rsig::Hook(ControllerRva,ControllerSig,sizeof ControllerSig,reinterpret_cast<void*>(&ControllerMode),reinterpret_cast<void**>(&isController));
    if(!activityIn)activityIn=d2rsig::Hook(ActivityRva,ActivitySig,sizeof ActivitySig,reinterpret_cast<void*>(&OnActivity),reinterpret_cast<void**>(&originalActivity));
    if(!requestIn)requestIn=d2rsig::Hook(RequestRva,RequestSig,sizeof RequestSig,reinterpret_cast<void*>(&OnRequest),reinterpret_cast<void**>(&originalRequest));
    if(!dispatchIn)dispatchIn=d2rsig::Hook(DispatchRva,DispatchSig,sizeof DispatchSig,reinterpret_cast<void*>(&OnDispatch),reinterpret_cast<void**>(&originalDispatch));
    if(!focusIn)focusIn=d2rsig::Hook(FocusRva,FocusSig,sizeof FocusSig,reinterpret_cast<void*>(&OnFocus),reinterpret_cast<void**>(&originalFocus));
    if(!cursorSnapIn)cursorSnapIn=d2rsig::Hook(CursorSnapRva,CursorSnapSig,sizeof CursorSnapSig,reinterpret_cast<void*>(&CursorSnap),reinterpret_cast<void**>(&originalCursorSnap));
    bool complete=activityIn&&requestIn&&controllerIn&&dispatchIn&&focusIn&&cursorSnapIn;
    for(int i=0;i<6;++i) {
        if(!pointerIn[i])pointerIn[i]=d2rsig::Hook(PointerRvas[i],i?ButtonSig:MoveSig,i?sizeof ButtonSig:sizeof MoveSig,
            reinterpret_cast<void*>(pointerHooks[i]),reinterpret_cast<void**>(&originalPointer[i]));
        complete&=pointerIn[i];
    }
    if(complete){ready.store(true,std::memory_order_release);framevr::logging::Write("Frame panels: controller layout retained; exact laser focus without controller cursor snapping and event consumption installed");}
}
}
