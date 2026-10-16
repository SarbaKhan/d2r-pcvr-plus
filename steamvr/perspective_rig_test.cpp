#include "perspective_rig.h"
#include "diorama_mr.h"
#include "diorama_placement.h"
#include "mouse_cursor.h"
#include "../cleanroom/camera/mat4.h"
#include <cassert>
#include <limits>
using namespace framevr;
void Near(float a,float b){assert(std::isfinite(a)&&std::fabs(a-b)<.0002f);}
void Same(math::Vec a,math::Vec b){Near(a.x,b.x);Near(a.y,b.y);Near(a.z,b.z);}
math::Vec Camera(const float* m){return {-m[12]*m[0]-m[13]*m[1]-m[14]*m[2],-m[12]*m[4]-m[13]*m[5]-m[14]*m[6],-m[12]*m[8]-m[13]*m[9]-m[14]*m[10]};}
void PlacementTests(){
    using namespace placement;
    Controls c;
    auto poll=[&](uint16_t b,uint64_t t,bool allowed=true,uint32_t epoch=1){return c.Poll(0,true,b,allowed,epoch,t);};
    assert(c.heightCm==74);
    poll(0,0);assert(poll(Back,10)==0);assert(poll(0,100)==Back);
    assert(poll(0,120)==Back);assert(poll(0,151)==0); // one deferred tap pulse
    poll(Back,200);poll(Back,400);poll(Back,600);poll(Back,700);
    assert(c.owner==0);assert(poll(Back|Up,710)==0);assert(c.heightCm==84);
    poll(Back|Up,720);assert(c.heightCm==84); // no auto-repeat
    poll(Back,730);poll(Back|Right,740);assert(c.heightCm==85);
    poll(Back,750);poll(Back|Left,760);assert(c.heightCm==84);
    poll(Back,770);poll(Back|Down,780);assert(c.heightCm==74);
    poll(Back|Up|Down,790);assert(c.heightCm==74); // opposing coarse keys
    assert(poll(Down,800)==0);assert(c.owner==-1); // held D-pad cannot leak at release
    assert(poll(Down,810)==0);poll(0,820);assert(poll(Down,830)==Down);
    poll(0,840);poll(Back,850);assert(poll(Back|Right,860,false)==0);
    assert(poll(Back|Right,870)==0);assert(poll(0,880)==0);assert(c.owner==-1);
    // UI epoch change cancels short taps rather than replaying a map toggle.
    poll(Back,900);poll(Back,920,true,2);assert(poll(0,930,true,2)==0);
    // Held at entry/reconnect requires a neutral observation first.
    Controls entry;assert(entry.Poll(0,true,Back,true,0,0)==0);
    assert(entry.Poll(0,true,Back,true,0,100)==0);assert(entry.owner==-1);
    entry.Poll(0,true,0,true,0,110);entry.Poll(0,true,Back,true,0,120);
    entry.Poll(0,true,Back,true,0,300);entry.Poll(0,true,Back,true,0,500);entry.Poll(0,true,Back,true,0,620);
    assert(entry.owner==0);entry.Expire(1000);assert(entry.owner==-1);
    assert(entry.Poll(0,true,Back,true,0,1010)==0);entry.Poll(0,false,0,true,0,1020);assert(!entry.Pending());
    assert(entry.Poll(0,true,Back,true,0,1030)==0);
    // Only one pad may place the table; idle other slots do not cancel it.
    Controls multi;for(int i=0;i<2;++i)multi.Poll(i,true,0,true,0,0);
    for(uint64_t t:{10,200,400,510})multi.Poll(0,true,Back,true,0,t);
    multi.Poll(1,true,0,true,0,520);assert(multi.owner==0);
    assert(multi.Poll(1,true,Back|Up,true,0,530)==0);assert(multi.heightCm==74);
    multi.Poll(1,false,0,true,0,540);assert(multi.owner==0);
    multi.CancelAll();assert(multi.owner==-1);assert(multi.Poll(0,true,Back,false,0,550)==0);
    // Clamp and exact centimetre arithmetic, other buttons untouched.
    c=Controls{};c.Configure(1.99f);poll(0,0);
    for(uint64_t t:{10,200,400,510})poll(Back,t);
    assert(poll(Back|Up|0x1000,520)==0x1000);assert(c.heightCm==200);
    c.heightCm=20;poll(Back,530);poll(Back|Down,540);assert(c.heightCm==20);
    // Floor-height placement ignores head height and preserves horizontal distance.
    for(float y:{.8f,1.7f,2.f}) {
        const auto p=perspective::TablePosition({2,y,3},0,1.2f,0,.74f);
        Same(p,{2,.74f,1.8f});
    }
    const auto p=perspective::TablePosition({0,9,0},1.57079632679f,1.2f,-.1f,.84f);
    Same(p,{-1.2f,.74f,0});
}
int main(){
    for(int mode=1;mode<=4;++mode) {
        assert(mousecursor::MenuVisible(mode,true,true)==(mode!=4));
        assert(!mousecursor::MenuVisible(mode,false,true));
        assert(!mousecursor::MenuVisible(mode,true,false));
    }

    PlacementTests();
    using namespace perspective;
    assert(dioramamr::Enabled(3,true,true,true,true,false));
    for(int mode:{1,2,4})assert(!dioramamr::Enabled(mode,true,true,true,true,false));
    assert(!dioramamr::Enabled(3,false,true,true,true,false));
    assert(!dioramamr::Enabled(3,true,false,true,true,false));
    assert(!dioramamr::Enabled(3,true,true,false,true,false));
    assert(!dioramamr::Enabled(3,true,true,true,false,false));
    assert(!dioramamr::Enabled(3,true,true,true,true,true));
    // Opaque white must stay white, not disappear as it would with color-keying.
    uint8_t pixel[4];const uint8_t white[]={255,255,255,0},black[]={0,0,0,0};
    mousecursor::Pixel(white,white,pixel);assert(pixel[0]==255&&pixel[1]==255&&pixel[2]==255&&pixel[3]==255);
    mousecursor::Pixel(black,white,pixel);assert(pixel[3]==0);
    mousecursor::Pixel(black,black,pixel);assert(pixel[3]==255&&pixel[0]==0);
    const uint8_t dark[]={10,20,30,0},light[]={137,147,157,0};
    mousecursor::Pixel(dark,light,pixel);assert(pixel[3]==128&&pixel[0]==60&&pixel[1]==40&&pixel[2]==20);
    // At any scale/orientation the hotspot must remain at the click, even on
    // non-square art with an off-center fingertip.
    for(float scale:{.001f,.02f})for(float yaw:{-.5f,1.f}) {
        const auto q=math::Yaw(yaw);const math::Vec hit{1,2,-3};
        const auto center=hit+math::Rotate(q,mousecursor::CenterOffset(96,80,7,3,scale));
        const auto tip=center+math::Rotate(q,{(7-48)*scale,(40-3)*scale,0});Same(tip,hit);
    }
    assert(KeyMode(1)==1&&KeyMode(2)==2&&KeyMode(3)==4&&KeyMode(4)==3);
    assert(Supported(4)&&!External(4)&&Supported(3)&&External(3));
    assert(Next(4)==1&&Next(1)==2&&Next(2)==3&&Next(3)==4);
    for(int mode:{1,2,3,4})for(bool panel:{false,true})
        assert(MouseVisible(mode,panel)==(mode==1||((mode==2||mode==3)&&panel)));
    for(float units:{1.f,3.8f,20.f})for(float yaw:{-2.f,0.f,1.2f})for(float down:{-.4f,.25f,.95f}){
        const math::Vec reference{2,1.6f,3},hero{42,4,-7};
        const auto rig=Orbit(hero,6,12,yaw,down,units,reference,.3f);
        assert(Valid(rig));
        Pose head{reference,math::Yaw(.3f)};
        auto left=head,right=head;
        left.p=left.p+math::Rotate(head.q,{-.032f,0,0});
        right.p=right.p+math::Rotate(head.q,{.032f,0,0});
        const auto l=Transform(rig,left),r=Transform(rig,right),c=Transform(rig,head);
        Same((l.p+r.p)*.5f,c.p);
        const auto separation=r.p-l.p;
        Near(std::sqrt(math::Dot(separation,separation)),.064f*units);
        Same(math::Rotate(l.q,{0,0,-1}),math::Rotate(r.q,{0,0,-1}));
        float v[16],inverse[16],projection[16];math::View(c.p,c.q,v);
        Same(Camera(v),c.p);assert(d2rcam::m4::Inverse(v,inverse));
        math::Projection(-.8f,.8f,-.7f,.7f,.15f,projection);
        float o[3],d[3];assert(d2rcam::m4::RayThrough(inverse,projection,1600,1200,800,600,o,d));
        Same({d[0],d[1],d[2]},math::Rotate(c.q,{0,0,-1}));
        auto moved=head;moved.p=moved.p+math::Vec{.1f,-.2f,.3f};
        Same(Transform(rig,moved).p-c.p,math::Rotate(rig.rotation,{.1f,-.2f,.3f})*units);
        // Turning the head changes orientation, not the orbit pivot/anchor.
        auto turned=head;turned.q=math::Multiply(head.q,Pitch(.4f));
        Same(Transform(rig,turned).p,c.p);
        Same(rig.anchor,c.p);
        // Neither head rotation nor translation enters the mouse camera.
        const auto mouse=MouseCamera(rig,.3f);
        math::View(mouse.p,mouse.q,v);assert(d2rcam::m4::Inverse(v,inverse));
        for(float u:{.1f,.5f,.9f})for(float sy:{.1f,.5f,.9f}) {
            const auto direction=MouseDirection(mouse,projection[0],projection[5],u,sy);
            assert(d2rcam::m4::RayThrough(inverse,projection,1600,1200,u*1600,sy*1200,o,d));
            Same({o[0],o[1],o[2]},mouse.p);Same({d[0],d[1],d[2]},direction);
            const auto marker=MouseMarker(mouse,direction,hero.y,12);
            // The displayed marker is on exactly the same world ray as picking.
            Same(math::Normalized(marker-mouse.p),direction);
            const auto tracking=ToTracking(rig,marker);
            Same(Transform(rig,{tracking,head.q}).p,marker);
            // Moving/turning the headset changes the rendered view of the marker,
            // but cannot change its world position or the mouse's picking ray.
            const auto movedEye=Transform(rig,moved),turnedEye=Transform(rig,turned);
            assert(math::Dot(movedEye.p-mouse.p,movedEye.p-mouse.p)>.001f);
            assert(math::Dot(math::Rotate(turnedEye.q,{0,0,-1})-math::Rotate(mouse.q,{0,0,-1}),
                             math::Rotate(turnedEye.q,{0,0,-1})-math::Rotate(mouse.q,{0,0,-1}))>.01f);
            Same(MouseCamera(rig,.3f).p,mouse.p);
        }
        const auto a=MouseDirection(mouse,projection[0],projection[5],.2f,.5f);
        const auto b=MouseDirection(mouse,projection[0],projection[5],.8f,.5f);
        assert(math::Dot(a-b,a-b)>.1f);
    }
    // A horizontal tabletop stays fixed while the eye translates/rotates and
    // while sampled game content follows the hero. Real IPD scales with the world.
    const math::Vec table{1,.9f,-.65f},world{40,3,20};
    for(float yaw:{-1.f,0.f,1.2f}) {
        const auto r=Table(world,table,yaw,48/.7f);
        assert(Valid(r));Same(ToTracking(r,world),table);
        Same(math::Rotate(r.rotation,{0,1,0}),{0,1,0});
        const auto l=Transform(r,{{-.032f,1.4f,0},{0,0,0,1}});
        const auto h=Transform(r,{{.032f,1.4f,0},{0,0,0,1}});
        Near(std::sqrt(math::Dot(h.p-l.p,h.p-l.p)),.064f*r.units);
        const auto moved=Table(world+math::Vec{2,0,3},table,yaw,r.units);
        Same(ToTracking(moved,world+math::Vec{2,0,3}),table);
    }
    const math::Vec target{1,0,0};math::Vec a{},b{};
    for(int i=0;i<60;++i)a=Follow(a,target,1.f/60,.15f,5);
    for(int i=0;i<120;++i)b=Follow(b,target,1.f/120,.15f,5);
    Same(a,b);Same(Follow({}, {100,0,0},.01f,.15f,5),{100,0,0});
    const auto limited=Follow({}, {10,0,0},0,.15f,3);
    Near(limited.x,7);
    auto invalid=Orbit({0,0,0},6,12,0,0,1,{0,0,0},0);
    invalid.units=0;assert(!Valid(invalid));invalid.units=std::numeric_limits<float>::quiet_NaN();assert(!Valid(invalid));
    invalid.units=1;invalid.rotation.w=0;assert(!Valid(invalid));
}
