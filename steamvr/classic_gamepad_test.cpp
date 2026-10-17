#include "classic_gamepad.h"
#include <cassert>
#include <limits>
using namespace pcvrplus::gamepad;
int main(){
    // Viewer-relative walk: opposite side reverses up; quarter turn gives left.
    float x=0,y=1;RotateWalk(x,y,3.14159265f);
    assert(std::fabs(x)<1e-6f&&std::fabs(y+1)<1e-6f);
    x=0;y=.5f;RotateWalk(x,y,1.57079633f);
    assert(std::fabs(x+.5f)<1e-6f&&std::fabs(y)<1e-6f);
    for(float angle:{-3.f,-1.f,0.f,1.f,3.f}) {
        x=.3f;y=.4f;RotateWalk(x,y,angle);
        assert(std::fabs(x*x+y*y-.25f)<1e-6f);
        RotateWalk(x,y,-angle);
        assert(std::fabs(x-.3f)<1e-6f&&std::fabs(y-.4f)<1e-6f);
        x=y=0;RotateWalk(x,y,angle);assert(x==0&&y==0);
    }
    assert(!CameraView(false,true,3)); // F4 right stick remains native
    assert(!CameraView(false,true,1));assert(CameraView(false,true,2));
    assert(!CameraView(false,false,4)); // immersive VR never enters this path
    assert(!CameraView(true,false,1));assert(CameraView(true,false,2));assert(CameraView(true,false,3));
    assert(Axis(.19f,.2f)==0&&Axis(-.2f,.2f)==0);
    assert(Axis(1.f,.2f)==1&&Axis(-1.f,.2f)==-1);
    assert(Axis(std::numeric_limits<float>::quiet_NaN(),.2f)==0);
    Camera c;c.Poll(2,true,1,1,true,102,7,1000);
    c.Poll(0,true,0,0,false,102,7,1000); // idle first slot must not erase active third slot
    auto d=c.Step(true,102,7,1010,.01f,.2f,120,90,false);
    assert(std::fabs(d.yaw-1.2f)<1e-5&&std::fabs(d.pitch+.9f)<1e-5);
    c.Poll(0,false,0,0,false,102,7,1010);assert(c.owner==2);
    c.Poll(2,true,0,0,false,102,7,1011);d=c.Step(true,102,7,1011,.01f,.2f,120,90,false);
    assert(d.yaw==0&&d.pitch==0); // release has no smoothing tail
    c.Poll(2,true,1,1,true,102,7,1012);
    d=c.Step(false,102,7,1013,.01f,.2f,120,90,false);assert(d.yaw==0&&c.owner==-1); // menu/focus gate
    c.Poll(2,true,1,1,true,102,7,1014);
    d=c.Step(true,102,8,1015,.01f,.2f,120,90,false);assert(d.yaw==0); // opened and closed between ticks
    c.Poll(2,true,1,1,true,102,8,1016);
    d=c.Step(true,103,8,1017,.01f,.2f,120,90,false);assert(d.yaw==0); // view transition
    c.Poll(2,true,1,1,true,103,8,1018);
    d=c.Step(true,103,8,1200,.01f,.2f,120,90,false);assert(d.yaw==0); // no more polls
    c.Poll(2,true,1,1,true,103,8,1201);c.Poll(2,false,0,0,false,103,8,1202);assert(c.owner==-1);
    float angle[2]{};
    for(int run=0;run<2;++run){
        const int fps=run?120:60;Camera camera;
        for(int i=0;i<fps;++i){
            camera.Poll(0,true,1,1,true,202,1,i*1000/fps);
            d=camera.Step(true,202,1,i*1000/fps,1.f/fps,.2f,120,90,true);
            angle[run]+=d.yaw;assert(d.pitch>0);
        }
    }
    assert(std::fabs(angle[0]-120)<.001f&&std::fabs(angle[1]-120)<.001f);
}
