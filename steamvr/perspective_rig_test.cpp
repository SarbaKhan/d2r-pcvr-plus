#include "perspective_rig.h"
#include "../cleanroom/camera/mat4.h"
#include <cassert>
#include <limits>
using namespace framevr;
void Near(float a,float b){assert(std::isfinite(a)&&std::fabs(a-b)<.0002f);}
void Same(math::Vec a,math::Vec b){Near(a.x,b.x);Near(a.y,b.y);Near(a.z,b.z);}
math::Vec Camera(const float* m){return {-m[12]*m[0]-m[13]*m[1]-m[14]*m[2],-m[12]*m[4]-m[13]*m[5]-m[14]*m[6],-m[12]*m[8]-m[13]*m[9]-m[14]*m[10]};}
int main(){
    using namespace perspective;
    assert(KeyMode(1)==1&&KeyMode(2)==2&&KeyMode(3)==4&&KeyMode(4)==0);
    assert(Supported(4)&&!External(4)&&!Supported(3));
    assert(Next(4)==1&&Next(1)==2&&Next(2)==4);
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
    }
    auto invalid=Orbit({0,0,0},6,12,0,0,1,{0,0,0},0);
    invalid.units=0;assert(!Valid(invalid));invalid.units=std::numeric_limits<float>::quiet_NaN();assert(!Valid(invalid));
    invalid.units=1;invalid.rotation.w=0;assert(!Valid(invalid));
}
