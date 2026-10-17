#include "spell_aim_assist.h"
#include <cassert>
#include <cstdio>
using namespace framevr;
using namespace aimassist;
Shot ShotAt(int mode=1,bool planar=true) {
    Shot s;s.kind=Kind::Spell;s.skill=47;s.aimAssist=mode;s.planar=planar;
    s.origin={10,1.6f,10};s.direction={0,0,1};s.player={10,0,10};s.unitsPerMetre=4;
    s.session=s.epoch=s.activation=s.binding=1;s.at=100;return s;
}
Candidate Enemy(const Shot& s,uint32_t id,float yaw,float distance=6,float radius=.1f) {
    Vec c=s.origin+Vec{std::sin(yaw*Degrees),0,std::cos(yaw*Degrees)}*distance;
    return Evaluate(s,id,c-Vec{0,.4f,0},c+Vec{0,.4f,0},radius);
}
int main() {
    auto s=ShotAt();
    assert(!Enemy(ShotAt(0),1,0).valid&&!Enemy(ShotAt(3),1,0).valid);
    assert(Enemy(s,1,0).direct&&Enemy(s,1,3).valid&&!Enemy(s,1,8).valid);
    assert(Enemy(ShotAt(2),1,12).valid&&!Enemy(ShotAt(2),1,15).valid);
    // A small enemy step defeated the old nearly tangent correction. Aim
    // farther inside, while keeping the correction bounded at the cone edge.
    for(int mode:{1,2}) {
        const auto shot=ShotAt(mode);
        const Vec center=shot.origin+Vec{std::sin(5*Degrees),0,std::cos(5*Degrees)}*6;
        const auto assisted=Evaluate(shot,1,center-Vec{0,.4f,0},center+Vec{0,.4f,0},.3f);
        assert(assisted.valid&&!assisted.direct);
        float entry=0;
        const Vec shifted=center+Vec{.05f,0,0};
        assert(CapsuleEntry(shot.origin,shot.origin+assisted.direction*Range,shifted-Vec{0,.4f,0},shifted+Vec{0,.4f,0},.3f,entry));
        if(mode==2)assert(math::Dot(assisted.direction,math::Normalized(center-shot.origin))>.999999f);
    }
    assert(!Enemy(ShotAt(1),1,10).valid&&Enemy(ShotAt(2),1,10).valid);
    const auto boundary=Enemy(ShotAt(2),1,12.5f);
    assert(boundary.valid&&boundary.angle<=Limit(2));
    assert(!Enemy(s,1,180).valid&&!Enemy(s,1,0,14).valid);
    auto invalid=s;invalid.direction={};assert(!Enemy(invalid,1,0).valid);
    invalid=s;invalid.origin.x=NAN;assert(!Enemy(invalid,1,0).valid);
    assert(!Evaluate(s,1,{10,1,16},{10,2,16},NAN).valid);
    assert(!Evaluate(s,1,{10,2,16},{10,1,16},.3f).valid);
    // Across yaw/pitch, precise shots remain bit-for-bit unchanged. Assisted
    // shots stay within the angular budget and truly intersect the capsule.
    for(int mode:{1,2})for(int yaw=0;yaw<360;yaw+=5)for(int pitch:{-70,-30,0,30,70}) {
        s=ShotAt(mode,false);
        s.direction={std::sin(yaw*Degrees)*std::cos(pitch*Degrees),std::sin(pitch*Degrees),std::cos(yaw*Degrees)*std::cos(pitch*Degrees)};
        const Vec center=s.origin+s.direction*6;
        auto exact=Evaluate(s,1,center-Vec{0,.2f,0},center+Vec{0,.2f,0},.15f);
        assert(exact.valid&&exact.direct&&exact.direction.x==s.direction.x&&exact.direction.y==s.direction.y&&exact.direction.z==s.direction.z);
        const Vec side={std::cos(yaw*Degrees),0,-std::sin(yaw*Degrees)};
        const Vec offset=center+side*.4f;
        const auto near=Evaluate(s,2,offset-Vec{0,.2f,0},offset+Vec{0,.2f,0},.15f);
        assert(near.valid&&!near.direct&&near.angle<=Limit(mode));
        float entry=0;assert(CapsuleEntry(s.origin,s.origin+near.direction*Range,offset-Vec{0,.2f,0},offset+Vec{0,.2f,0},.15f,entry));
        assert(math::Dot(s.direction,near.direction)>=std::cos(Limit(mode))-.00001f);
    }
    // Planar assistance refuses low enemies; free aim can pitch down when
    // the required correction is small. Charged Bolt aims the footprint.
    s=ShotAt(2);Vec low={10.4f,.8f,16},high={10.4f,1.1f,16};
    assert(!Evaluate(s,1,low,high,.2f).valid);
    s.planar=false;auto down=Evaluate(s,1,low,high,.2f);
    assert(down.valid&&down.direction.y<0);
    s.planar=true;s.skill=38;
    auto ground=Evaluate(s,1,low,high,.2f);assert(ground.valid&&ground.direction.y==0);
    s=ShotAt(2);
    for(float height:{1.3f,1.6f,1.9f}) {
        auto c=Evaluate(s,1,{10.5f,height,16},{10.5f,height+.3f,16},.4f);
        assert(c.valid&&c.direction.y==0);
    }
    // Closest direction wins; direct aim overrides preference. Tie order is
    // stable, and a previous target never gets extra acquisition tolerance.
    s=ShotAt();auto a=Enemy(s,1,3),b=Enemy(s,2,2.8f),direct=Enemy(s,3,0);
    Selection ordinary(0,false);ordinary.Accept(a);ordinary.Accept(b);assert(ordinary.Best().id==2);
    Selection sticky(1,true);sticky.Accept(b);sticky.Accept(a);assert(sticky.Best().id==1);
    sticky.Accept(direct);assert(sticky.Best().id==3&&sticky.Best().direct);
    Selection tie(0,false);tie.Accept(Enemy(s,9,3));tie.Accept(Enemy(s,8,3));assert(tie.Best().id==8);
    tie.Accept(Enemy(s,0,20));assert(tie.Best().id==8);
    // Independent hands and invalidation on release, settings/skill changes,
    // tracking stalls, turn/area changes, equipment changes, and clock reset.
    History hands[2];hands[0].Begin(s);hands[0].Finish(a);
    auto right=hands[0].Begin(s),left=hands[1].Begin(s);
    for(auto* pick:{&right,&left}){pick->Accept(b);pick->Accept(a);}
    assert(right.Best().id==1&&left.Best().id==2);
    for(int event=0;event<11;++event) {
        History h;h.Begin(s);h.Finish(a);auto next=s;
        switch(event){
            case 0:++next.session;break;case 1:++next.epoch;break;case 2:++next.activation;break;
            case 3:++next.binding;break;case 4:++next.equipment;break;case 5:++next.skill;break;
            case 6:next.aimAssist=2;break;case 7:next.planar=false;break;
            case 8:next.at+=751;break;case 9:--next.at;break;case 10:next.hand=1;break;
        }
        auto pick=h.Begin(next);pick.Accept(a);pick.Accept(b);assert(pick.Best().id==2);
    }
    History lost;lost.Begin(s);lost.Finish(a);lost.Begin(s);lost.Finish({});
    auto reacquire=lost.Begin(s);reacquire.Accept(a);reacquire.Accept(b);assert(reacquire.Best().id==2);
    // The selected corrected path, not just the original ray, must be clear.
    // Reuse the collision supercover: closed doors/unloaded cells reject it.
    Selection visible(0,false);
    const auto end=s.origin+a.direction*a.distance;
    auto clear=[&](bool wall){return WallEntry(GridPoint(s,s.origin),GridPoint(s,end),s.unitsPerMetre,[&](int,int z){return wall&&z==26;})>=1;};
    assert(clear(false)&&!clear(true));
    if(clear(true))visible.Accept(a);assert(!visible.Best().valid);
    if(clear(false))visible.Accept(a);assert(visible.Best().id==1);
    // Selection cannot mutate a saved launch. Steering requires an explicit
    // per-flight update, independent of the other hand's current selection.
    Flight flight;flight.shot=s;flight.shot.direction=a.direction;
    visible.Accept(direct);assert(flight.shot.direction.x==a.direction.x);
    // Moving-target regression: the launch ray was a direct hit, but the
    // enemy crosses it before arrival. Only Strong follows that same enemy.
    for(int mode:{0,1,2}) {
        auto shot=ShotAt(mode);shot.direction={0,0,1};
        Flight moving;moving.shot=shot;moving.alive=true;moving.speed=13;
        Vec center=shot.origin+Vec{0,0,8};
        auto target=Evaluate(shot,42,center-Vec{0,.4f,0},center+Vec{0,.4f,0},.25f);
        Homing homing;homing.Arm(shot,target,7,center);
        bool hit=false;
        for(int tick=1;tick<=28;++tick) {
            center.x=shot.origin.x+1.5f*tick*.04f;
            const Vec before=moving.Point();Vec direction{};
            const auto previous=moving.Direction();
            if(homing.Step(moving,center-Vec{0,.4f,0},center+Vec{0,.4f,0},.25f,true,.04f,direction)) {
                assert(math::Dot(previous,direction)>=std::cos(Homing::Rate*.04f)-.00001f);
                moving.Redirect(direction);
                assert(Length(moving.Point()-before)<.00001f); // no jump when bending
            }
            moving.distance+=moving.speed*.04f;
            const Vec after=moving.Point();
            assert(std::fabs(Length(after-before)-.52f)<.00001f&&after.y==shot.origin.y);
            assert(homing.turned<=Homing::Budget+.00001f);
            float entry=0;
            if(CapsuleEntry(before,after,center-Vec{0,.4f,0},center+Vec{0,.4f,0},.25f,entry)){hit=true;break;}
        }
        assert(hit==(mode==2));
        assert(moving.shot.origin.x==shot.origin.x&&moving.shot.direction.x==0);
        if(mode==2) {
            assert(homing.target==42&&homing.classId==7);
            moving.updatedAt=100;
            assert(Length(moving.TailAt(120)-(moving.Point()+moving.Direction()*.26f))<.00001f);
            float matrix[16]={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
            SpellMatrix(moving,100,matrix);
            assert(Length(Vec{matrix[8],matrix[9],matrix[10]}-moving.Direction())<.00001f);
        }
    }
    // Wall/death/teleport/height loss ends steering permanently; no new
    // target is acquired. Free aim can bend vertically, planar cannot.
    s=ShotAt(2);Vec center=s.origin+Vec{.3f,0,6};
    const auto target=Evaluate(s,7,center-Vec{0,.4f,0},center+Vec{0,.4f,0},.2f);
    assert(target.valid);
    for(int reason=0;reason<5;++reason) {
        Flight f;f.shot=s;Homing h;h.Arm(s,target,2,center);Vec direction{};
        Vec c=center;
        if(reason==1)c.x+=2; // teleport
        if(reason==2)c.y-=2; // below plane
        if(reason==3){f.distance=7;c=s.origin+Vec{.3f,0,6};} // behind flight
        if(reason==4)h.age=Homing::Lifetime;
        assert(!h.Step(f,c-Vec{0,.4f,0},c+Vec{0,.4f,0},.2f,reason!=0,.04f,direction)&&!h.active);
        assert(!h.Step(f,center-Vec{0,.4f,0},center+Vec{0,.4f,0},.2f,true,.04f,direction));
    }
    {
        Flight f;f.shot=s;Homing h;h.Arm(s,target,2,center);Vec direction{};
        h.turned=Homing::Budget-.001f;
        assert(h.Step(f,center-Vec{0,.4f,0},center+Vec{0,.4f,0},.2f,true,.04f,direction));
        assert(h.turned<=Homing::Budget+.000001f);
        assert(!h.Step(f,center-Vec{0,.4f,0},center+Vec{0,.4f,0},.2f,true,.04f,direction));
        f.shot.planar=false;center.y+=.3f;h.Arm(f.shot,target,2,center);
        assert(h.Step(f,center-Vec{0,.4f,0},center+Vec{0,.4f,0},.2f,true,.04f,direction)&&direction.y>0);
        f.shot.skill=38;h.Arm(f.shot,target,2,center);assert(!h.active);
        f.Redirect(direction);assert(!f.redirected); // ground path stays native
        f.shot.kind=Kind::Arrow;h.Arm(f.shot,target,2,center);assert(!h.active);
        f.Redirect(direction);assert(!f.redirected);
    }
    puts("Spell assist: bounded launch correction and Strong homing, moving targets, continuous flight, planar/free/ground paths, target loss and lifecycle passed");
}
