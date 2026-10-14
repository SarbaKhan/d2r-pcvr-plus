#include "gameplay.h"
#include "melee_contact.h"
#include "../vr/shield_pose.h"
#include "../vr/native_skill_dispatch.h"
#include <cassert>
#include <cstdio>
using namespace framevr;
struct Sim {
    gameplay::Engine engine;gameplay::Input in;gameplay::Output out;
    Sim() {
        in.active=true;in.tracked[0]=in.tracked[1]=in.aimTracked=true;in.weapon=gameplay::Weapon::Bow;in.equipment=1;
        in.head.p={0,1.65f,0};in.hand[0].p={0,1.3f,-.4f};in.hand[1].p={0,1.3f,-.5f};in.aim=in.hand[0];Step(2);
    }
    void Step(int n=1){while(n--){in.time+=.02;out=engine.Step(in);}}
    void Select(int id){engine.Select(2);in.selectedSkill=id;in.selectedAvailable=true;in.skillContext=7;Step(2);}
    void Draw(){in.trigger=1;Step();assert(out.bowDrawing);for(int i=0;i<10;++i){in.hand[0].p.z+=.04f;Step();}}
};
int main() {
    for(int id:{6,7,11,12,16,21,22,26,27,31}) {
        Sim s;s.Select(id);s.Draw();assert(s.out.skill==-1&&!s.out.attack&&!s.out.bowRelease);
        s.in.trigger=0;s.Step();assert(s.out.nativeBowRelease&&s.out.skill==2&&!s.out.bowRelease&&!s.out.attack);
        assert(s.out.rayDirection.z<-.99f);s.in.hand[0].p.x+=.1f;s.Step();assert(s.out.rayDirection.z<-.99f);
        s.Step(5);assert(!s.out.nativeBowRelease&&s.out.skill==-1);s.Step(30);assert(s.out.skill==-1);
    }
    for(int interruption=0;interruption<6;++interruption) {
        Sim s;s.Select(12);s.Draw();
        if(interruption==0)s.in.selectedSkill=6;
        if(interruption==1)++s.in.selectedOwner;
        if(interruption==2)++s.in.skillContext;
        if(interruption==3)s.in.selectedAvailable=false;
        if(interruption==4)s.in.tracked[1]=false;
        if(interruption==5)s.in.quickCast=true;
        s.Step();assert(!s.out.nativeBowRelease&&!s.out.bowRelease&&s.out.skill==-1);
        s.in.trigger=0;s.Step();assert(!s.out.nativeBowRelease&&!s.out.bowRelease);
    }
    {
        Sim s;s.Select(12);s.in.hand[0].p.x=.5f;s.Step(2);s.in.trigger=1;s.Step();assert(s.out.skill==2&&!s.out.bowDrawing);
        for(int i=0;i<10;++i){s.in.hand[0].p.x-=.05f;s.Step();assert(s.out.skill==2&&!s.out.bowDrawing);}
        s.in.trigger=0;s.Step();assert(!s.out.nativeBowRelease&&!s.out.bowRelease);
    }
    {
        Sim s;s.Select(12);s.in.hand[0].p.x=.5f;s.Step(2);s.in.trigger=1;s.Step();assert(s.out.skill==2);
        ++s.in.selectedOwner;s.Step();assert(s.out.skill==-1&&s.out.block);s.Step();assert(s.out.skill==-1);
    }
    {
        Sim s;s.Select(12);s.Draw();s.in.trigger=0;s.Step();assert(s.out.nativeBowRelease);
        s.in.quickCast=true;s.Step();assert(s.out.skill==-1);s.in.quickCast=false;s.Step();assert(s.out.skill==-1);
    }
    {
        Sim s;s.in.weapon=gameplay::Weapon::Melee;s.Step(2);s.Select(weaponskills::Smite);
        s.in.shieldEquipped=true;s.Step();assert(s.out.shieldContact&&s.out.meleeContact&&s.out.skill==-1);
        s.in.trigger=1;s.Step();assert(!s.out.shieldContact&&!s.out.meleeContact&&s.out.skill==2);
        s.in.trigger=0;s.in.shieldEquipped=false;s.Step();assert(!s.out.shieldContact);
    }
    {
        const weaponskills::ContactContext saved{3,4,5,6,2,-1};
        assert(weaponskills::Current(saved,saved,100,101));
        assert(!weaponskills::Current(saved,saved,100,99));
        assert(!weaponskills::Current(saved,saved,100,1601));
        assert(!weaponskills::Current(saved,saved,100,201,100));
        for(int change=0;change<6;++change) {
            auto current=saved;
            if(change==0)++current.session;
            if(change==1)++current.epoch;
            if(change==2)++current.assignment;
            if(change==3)++current.equipment;
            if(change==4)++current.slot;
            if(change==5)++current.owner;
            assert(!weaponskills::Current(saved,current,100,101));
        }
    }
    {
        weaponskills::Pulse p;assert(p.Start(2,12,-1,7,1));assert(p.Get(2,12,-1,7,1.02)==2);
        assert(!p.Start(2,12,-1,7,1.03));assert(p.Get(2,12,-1,8,1.04)==-1);
        assert(p.Start(2,12,-1,8,2));assert(p.Get(2,12,-1,8,1.9)==-1);
    }
    {
        using namespace skel::bow;
        auto bindWrist=Identity(),bindShield=Identity(),tracked=Identity(),result=Identity();
        Position(bindWrist,{2,1,0});Position(bindShield,{2,1.2f,.1f});
        bindShield.m[0]=-1; // the left attachment is allowed to be mirrored
        tracked=Rotation({0,0,1},{1,0,0});Position(tracked,{5,3,2});
        assert(skel::shield::Follow(bindWrist,bindShield,tracked,result));
        assert(Length(Position(result)-(Position(tracked)+Vector(tracked,{0,.2f,.1f})))<.0001f);
        assert(Dot({result.m[0],result.m[1],result.m[2]},Cross({result.m[4],result.m[5],result.m[6]},{result.m[8],result.m[9],result.m[10]}))<0);
        bindWrist.m[0]=0;assert(!skel::shield::Follow(bindWrist,bindShield,tracked,result));
    }
    {
        using ::melee::native::FunctionRange;using ::melee::native::FindFunction;
        const std::array<FunctionRange,3> ranges={FunctionRange{0x100,0x140,0x800},FunctionRange{0x200,0x280,0x810},FunctionRange{0x300,0x380,0x820}};
        assert(FindFunction(ranges,0x200,0x1000)==ranges[1]);
        assert(!FindFunction(ranges,0x210,0x1000).begin); // containing range cannot replace an exact entry
        assert(!FindFunction(ranges,0x200,0x250).begin);
        auto broken=ranges;broken[1].unwind=0x1000;assert(!FindFunction(broken,0x200,0x1000).begin);
        broken=ranges;broken[1].end=broken[1].begin+8;assert(!FindFunction(broken,0x200,0x1000).begin);
        broken=ranges;broken[2]={0x200,0x290,0x810};assert(!FindFunction(broken,0x200,0x1000).begin);
    }
    {
        std::array<uintptr_t,151> table{};table[1]=0x1000;table[3]=0x2000;table[150]=0x3000;
        assert(::melee::native::SmiteEntry(table,0x1000,0x2000)==0x3000);
        table[3]=0x4000;assert(!::melee::native::SmiteEntry(table,0x1000,0x2000));
        const uint8_t code[]={0x90,0xe8,0x0a,0,0,0,0xc3};
        assert(::melee::native::CallsTarget(code,0x1000,0x1010));assert(!::melee::native::CallsTarget(code,0x1000,0x1011));
        const uint8_t immediate[]={0x48,0xb8,0xe8,0x09,0,0,0,0,0,0,0xc3};
        assert(!::melee::native::CallsTarget(immediate,0x1000,0x1010));
        const uint8_t backward[]={0xe8,0xeb,0xff,0xff,0xff,0xc3};
        assert(::melee::native::CallsTarget(backward,0x1020,0x1010));
    }
    {
        framevr::melee::Engine e;framevr::melee::Sweep sweep;framevr::melee::Sample sample;
        sample.active=sample.shield=true;sample.skillSlot=2;sample.skillContext=1;sample.equipment=1;sample.unitsPerMetre=5;
        sample.player={0,0,0};sample.hand.p={0,1,0};sample.blade={{0,.8f,0},{0,1.2f,0},.15f};
        for(int i=0;i<20;++i){sample.time+=.02;assert(!e.Step(sample,sweep));}
        framevr::melee::HitSet hits;int bashes=0;
        for(int i=0;i<25;++i){sample.time+=.02;sample.hand.p.z-=.04f;sample.blade.base.z-=.04f;sample.blade.tip.z-=.04f;
            if(e.Step(sample,sweep)&&hits.Insert(1,sweep.stroke,UINT32_MAX))++bashes;}
        assert(bashes==1); // keep observing during wind-up: no second bash from this swing
        for(int i=0;i<10;++i){sample.time+=.02;sample.hand.p.z+=.04f;sample.blade.base.z+=.04f;sample.blade.tip.z+=.04f;
            if(e.Step(sample,sweep)&&hits.Insert(1,sweep.stroke,UINT32_MAX))++bashes;}
        assert(bashes==2);++sample.skillContext;sample.time+=.02;assert(!e.Step(sample,sweep));
        sample.active=false;sample.time+=.02;assert(!e.Step(sample,sweep));
    }
    std::puts("Weapon skills: release/cancellation/fallback, shield ownership/pose/strokes and native dispatch guards passed");
}
