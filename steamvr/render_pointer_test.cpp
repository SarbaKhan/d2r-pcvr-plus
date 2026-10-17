// Exercise the actual user32 detours from a game-like executable. No D2R,
// runtime, player files or graphics device are loaded by this fixture.
#include "render_resolution.h"
#include <MinHook.h>
#include <cassert>
#include <cstdio>
using namespace framevr::resolution;
static unsigned resizes=0;
static Size nativeSize{};
static LPARAM nativeMouse=0;
static LRESULT CALLBACK Native(HWND h,UINT m,WPARAM w,LPARAM l){
    if(m==WM_SIZE){++resizes;nativeSize={LOWORD(l),HIWORD(l)};}
    if(m>=WM_MOUSEFIRST&&m<=WM_MOUSELAST)nativeMouse=l;
    return DefWindowProcW(h,m,w,l);
}
static LRESULT CALLBACK Hook(HWND h,UINT m,WPARAM w,LPARAM l){
    LRESULT result=0;if(Handle(h,m,w,l,Native,result))return result;
    return Native(h,m,w,l);
}
static void Pump(){MSG m{};while(PeekMessageW(&m,nullptr,0,0,PM_REMOVE))DispatchMessageW(&m);}
int main(){
    assert(MH_Initialize()==MH_OK);
    WNDCLASSW cls{};cls.lpfnWndProc=Hook;cls.hInstance=GetModuleHandleW(nullptr);cls.lpszClassName=L"FramePointerFixture";
    assert(RegisterClassW(&cls));
    auto h=CreateWindowW(cls.lpszClassName,L"Pointer fixture",WS_OVERLAPPEDWINDOW,100,100,800,600,nullptr,nullptr,cls.hInstance,nullptr);
    auto other=CreateWindowW(cls.lpszClassName,L"Unrelated fixture",WS_OVERLAPPEDWINDOW,100,100,800,600,nullptr,nullptr,cls.hInstance,nullptr);
    assert(h&&other);RECT r{};assert(GetClientRect(h,&r));
    POINT origin{};assert(ClientToScreen(h,&origin));
    POINT otherOrigin{};assert(ClientToScreen(other,&otherOrigin));
    const uint64_t nativeCanvas=uint64_t(r.right)|(uint64_t(r.bottom)<<32);
    Attach(h);assert(UiSize(nativeCanvas)==nativeCanvas&&CursorSize(nativeCanvas)==nativeCanvas);
    Update(h,true,{2160,2160},{8192,8192},{uint32_t(r.right),uint32_t(r.bottom)},4);
    assert(UiSize(nativeCanvas)==nativeCanvas&&CursorSize(nativeCanvas)==nativeCanvas); // queued is not yet applied
    Pump();assert(UiSize(nativeCanvas)==(uint64_t(2560)|(uint64_t(1440)<<32)));
    POINT p{origin.x+r.right-1,origin.y+r.bottom-1};
    assert(ScreenToClient(h,&p)&&p.x==2559&&p.y==2559);
    assert(ClientToScreen(h,&p)&&p.x==origin.x+r.right-1&&p.y==origin.y+r.bottom-1);
    p={otherOrigin.x+r.right-1,otherOrigin.y+r.bottom-1};
    assert(ScreenToClient(other,&p)&&p.x==r.right-1&&p.y==r.bottom-1);
    // Regression: the entire 3840-wide native menu must reflow into a smaller
    // VR canvas, while pointer coordinates agree and OS client size stays real.
    const uint64_t wideDesktop=uint64_t(3840)|(uint64_t(2160)<<32);
    for(const auto size:{Size{1536,1536},Size{3072,3072},Size{4096,4096},Size{2160,2400}}) {
        Update(h,true,size,{8192,8192},nativeSize,1);Pump();
        assert((nativeSize==size));
        const auto canvas=UiSize(wideDesktop);
        assert(uint32_t(canvas)==size.w&&uint32_t(canvas>>32)==size.w*9/16);
        const auto bounds=CursorSize(nativeCanvas);
        assert(uint32_t(bounds)==size.w&&uint32_t(bounds>>32)==size.h);
        // Reproduce the native post-dispatch clamp: old desktop bounds make
        // bottom-row controls unreachable even when the ray/event XY is right.
        POINT bottom{LONG(size.w-100),LONG(uint32_t(canvas>>32)-40)};
        assert(std::clamp(bottom.y,0L,r.bottom-1)!=bottom.y);
        assert(std::clamp(bottom.x,0L,LONG(uint32_t(bounds))-1)==bottom.x);
        assert(std::clamp(bottom.y,0L,LONG(uint32_t(bounds>>32))-1)==bottom.y);
        p={origin.x+r.right-1,origin.y+r.bottom-1};
        assert(ScreenToClient(h,&p)&&p.x==LONG(size.w-1)&&p.y==LONG(size.h-1));
        assert(uint32_t(canvas>>32)<=size.h); // every UI pixel remains reachable
        RECT physical{};assert(GetClientRect(h,&physical)&&EqualRect(&physical,&r));
        // Real window events and polled mouse coordinates must hit the SAME
        // native button at the center, edges, and outside during a drag.
        for(POINT client:{POINT{r.right/2,r.bottom/2},POINT{r.right-1,r.bottom-1},POINT{-12,-8}}) {
            POINT polled{origin.x+client.x,origin.y+client.y};
            assert(ScreenToClient(h,&polled));
            for(UINT message:{WM_MOUSEMOVE,WM_LBUTTONDOWN,WM_LBUTTONUP,WM_RBUTTONDOWN,WM_RBUTTONUP,WM_XBUTTONDOWN}) {
                SendMessageW(h,message,0,MAKELPARAM(client.x,client.y));
                assert(short(LOWORD(nativeMouse))==polled.x&&short(HIWORD(nativeMouse))==polled.y);
                SendMessageW(other,message,0,MAKELPARAM(client.x,client.y));
                assert(short(LOWORD(nativeMouse))==client.x&&short(HIWORD(nativeMouse))==client.y);
            }
        }
        const auto screen=MAKELPARAM(origin.x+10,origin.y+20);
        SendMessageW(h,WM_MOUSEWHEEL,0,screen);assert(nativeMouse==screen);
        SendMessageW(h,WM_MOUSEHWHEEL,0,screen);assert(nativeMouse==screen);
    }
    Update(h,true,{2160,2160},{8192,8192},nativeSize,4);Pump();
    // The UI query may decrypt after the initial resize. Reflow on its owning
    // thread once it is hooked, without applying a different source size.
    auto beforeUi=resizes;UiSizeReady();assert(resizes==beforeUi);
    Pump();assert(resizes==beforeUi+1&&(nativeSize==Size{2560,2560}));
    // Reproduce native Apply invalidating its configuration behind our cached
    // logical size. The real module must defer/coalesce then re-send 2560.
    auto count=resizes;nativeSize={1920,1080};
    OptionsApplied();OptionsApplied();
    Update(h,true,{2160,2160},{8192,8192},{2560,2560},4);Pump();
    assert(resizes==count&&(nativeSize==Size{1920,1080}));
    Sleep(550);
    Update(h,true,{2160,2160},{8192,8192},{2560,2560},4);Pump();
    assert(resizes==count+1&&(nativeSize==Size{2560,2560}));
    for(int i=0;i<10;++i){Update(h,true,{2160,2160},{8192,8192},{2560,2560},4);Pump();}
    assert(resizes==count+1); // no self-sustaining resize loop
    p={origin.x+r.right-1,origin.y+r.bottom-1};
    assert(ScreenToClient(h,&p)&&p.x==2559&&p.y==2559);
    OptionsApplied(); // XR shutdown cancels pending recovery.
    Restore();Pump();p={origin.x+r.right-1,origin.y+r.bottom-1};
    assert(UiSize(nativeCanvas)==nativeCanvas&&UiSize(wideDesktop)==wideDesktop&&CursorSize(nativeCanvas)==nativeCanvas);
    beforeUi=resizes;UiSizeReady();Pump();assert(resizes==beforeUi);
    assert(ScreenToClient(h,&p)&&p.x==r.right-1&&p.y==r.bottom-1);
    SendMessageW(h,WM_MOUSEMOVE,0,MAKELPARAM(123,234));assert(nativeMouse==MAKELPARAM(123,234));
    count=resizes;OptionsApplied();Sleep(550);
    Update(h,false,{2160,2160},{8192,8192},{uint32_t(r.right),uint32_t(r.bottom)},4);Pump();
    assert(resizes==count);
    Update(h,true,{2160,2160},{8192,8192},{uint32_t(r.right),uint32_t(r.bottom)},0);Pump();
    assert(resizes==count); // Desktop never forces a VR resize.
    assert(MH_Uninitialize()==MH_OK);DestroyWindow(other);DestroyWindow(h);
    UnregisterClassW(cls.lpszClassName,cls.hInstance);
    std::puts("Native UI canvas, deferred reflow, real pointer hooks, inverse cursor warp, unrelated window and desktop restoration passed");
}
