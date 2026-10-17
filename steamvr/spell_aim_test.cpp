#include "spell_aim.h"
#include <cstdio>
#include <cstdlib>
#include <limits>
using namespace framevr;
static void Check(bool b,const char* why){if(!b){std::fprintf(stderr,"FAIL: %s\n",why);std::exit(1);}}
int main() {
    Check(spellaim::Directed(0,-1,8),"Teeth fan is directional without a primary missile field");
    Check(spellaim::Directed(0,36,0)&&spellaim::Directed(0,395,0),"primary missile behavior is independent of class/skill id");
    Check(spellaim::Directed(0,-1,19)&&spellaim::Directed(0,-1,10),"streams and initially aimed homing casts retain hand direction");
    for(int function:{22,28,56,60,63,68,123})Check(!spellaim::Directed(0,-1,function),"nova, ground, summon, corpse and buff behaviors retain native targeting");
    for(uint32_t flag:{1u<<4,1u<<20,1u<<24})Check(!spellaim::Directed(flag,12,8),"passive and unit/corpse-only skills never lose required targets");
    Check(!spellaim::Directed(0,-1,0)&&!spellaim::Directed(0,-1,999),"unknown behavior is not guessed to be projectile");
    spellaim::Cast context{};
    {
        spellaim::Scope cast(context,{true,.6f,.8f});
        {
            spellaim::Scope nested(context,{});
            Check(!context.directed,"nested non-projectile resolution cannot inherit target suppression");
        }
        Check(context.directed&&context.x==.6f,"nested return restores sampled direction for remaining fan resolution");
    }
    Check(!context.directed,"native targeting restored after the cast");
    for(int degree=-180;degree<=180;++degree) {
        const float angle=degree*3.14159265f/180;
        for(float pitch:{-.9f,0.f,.9f}) {
            const math::Vec direction{std::cos(angle)*std::cos(pitch),std::sin(pitch),std::sin(angle)*std::cos(pitch)};
            float x=0,y=0;
            Check(spellaim::Point(1200,1300,direction,x,y),"finite directional spell yields native aim point");
            const auto rounded=math::Normalized(math::Vec{float(int(x)-1200),0,float(int(y)-1300)});
            Check(rounded.x*std::cos(angle)+rounded.z*std::sin(angle)>.999f,"all horizontal aim angles stay within 2.6 degrees after native cell quantization");
        }
    }
    float x=5,y=7;
    for(int degree=-180;degree<=180;degree+=5) {
        const float angle=degree*3.14159265f/180;
        const spellaim::Cast cast{true,std::cos(angle),std::sin(angle)};
        // Native resolution can return the movement target without invoking
        // AttackPoint. Simulate forward, backward, strafing and idle results.
        for(float travel:{0.f,1.57079633f,3.14159265f,-1.57079633f})for(float distance:{0.f,8.f}) {
            const float playerX=1200.25f+std::cos(angle+travel)*distance;
            const float playerY=1300.75f+std::sin(angle+travel)*distance;
            x=playerX+std::cos(angle+travel)*3;y=playerY+std::sin(angle+travel)*3;
            Check(spellaim::Resolve(cast,playerX,playerY,x,y),"moving cast overrides every native resolver result");
            const auto actual=math::Normalized(math::Vec{x-playerX,0,y-playerY});
            Check(actual.x*cast.x+actual.z*cast.y>.99999f,"projectile aim is independent of forward, backward or sideways travel");
            int cx=int(playerX-3*cast.x),cy=int(playerY-3*cast.y);
            Check(spellaim::ControllerPoint(playerX,playerY,{cast.x,0,cast.y},cx,cy),"controller quick cast also replaces backward walking destination");
            const auto controller=math::Normalized(math::Vec{cx-playerX,0,cy-playerY});
            Check(controller.x*cast.x+controller.z*cast.y>.999f,"controller quick-cast integer target retains hand direction");
            Check(std::fabs(std::hypot(x-playerX,y-playerY)-40)<.001f,"cast point stays inside native command range");
        }
    }
    x=5;y=7;
    Check(!spellaim::Resolve({},1200,1300,x,y)&&x==5&&y==7,"non-projectile native targets are preserved");
    Check(spellaim::Resolve({true,23,42,true},1200,1300,x,y)&&x==23&&y==42,"shield contact keeps its exact point instead of projectile range");
    x=5;y=7;
    Check(!spellaim::Resolve({true,NAN,0},1200,1300,x,y)&&x==5&&y==7,"invalid cast aim preserves native result");
    Check(!spellaim::Resolve({true,1,0},NAN,1300,x,y)&&x==5&&y==7,"invalid player position preserves native result");
    Check(!spellaim::Point(0,0,{0,1,0},x,y)&&x==5&&y==7,"vertical aim cannot invent a horizontal direction");
    Check(!spellaim::Point(0,0,{NAN,0,1},x,y),"invalid direction is refused");
    int cx=12,cy=34;
    Check(!spellaim::ControllerPoint(NAN,0,{1,0,0},cx,cy)&&cx==12&&cy==34,"invalid controller position cannot change output");
    Check(!spellaim::ControllerPoint(65530,100,{1,0,0},cx,cy),"out-of-world controller target is refused before integer conversion");
    for(int type=0;type<18;++type)
        Check(spellaim::ControllerCast(type)==(type==7||type==8||type==9||type==10),"only native controller skill press/repeat owns a command");
    for(int degree=0;degree<360;++degree) {
        const float angle=degree*3.14159265f/180;
        Check(spellaim::ControllerPoint(4730.25f,4621.75f,{std::cos(angle),0,std::sin(angle)},cx,cy),"rotating hand yields a command target");
        // Native MapClick has reacquired a selected unit and its old point,
        // after our resolver returned. Cover left/right press and repeat flags.
        for(uint32_t flags:{5u,9u,6u,10u}) {
            spellaim::NativeCommand command{flags,0x1234,123,456,4730,4681};
            Check(spellaim::ApplyCommand({123,cx,cy},command),"final local command removes reacquired native target");
            Check(command.target==0&&command.flags==flags&&command.padding==0x1234&&command.actor==123,"only target fields change; native press/repeat and actor survive");
            const auto actual=math::Normalized(math::Vec{command.x-4730.25f,0,command.y-4621.75f});
            Check(actual.x*std::cos(angle)+actual.z*std::sin(angle)>.999f,"final command retains every intermediate heading across all four quadrants");
        }
    }
    spellaim::NativeCommand untouched{6,0,123,456,11,22};
    for(const auto aim:{spellaim::CommandAim{},spellaim::CommandAim{124,100,200},
                       spellaim::CommandAim{123,0,200},spellaim::CommandAim{123,100,65535}}) {
        Check(!spellaim::ApplyCommand(aim,untouched),"unowned, other actor and invalid command aim are refused");
        Check(untouched.target==456&&untouched.x==11&&untouched.y==22,"refused command keeps native targeting intact");
    }
    // Independently model the audited native command gate, not just the ray
    // formula. The previous 60-cell point passed only around the diagonals.
    const auto accepted=[](int px,int py,int tx,int ty) {
        return std::abs(tx-px)<=50&&std::abs(ty-py)<=50;
    };
    Check(!accepted(4734,4649,4794,4649)&&accepted(4734,4649,4776,4691),
          "old 60-cell aim reproduces cardinal rejection and diagonal acceptance");
    Check(!accepted(4734,4649,4790,4627)&&accepted(4734,4649,4769,4698),
          "observed stale versus server-created fireballs match native range gate");
    for(int degree=0;degree<360;++degree)for(float fraction:{0.f,.25f,.75f,.99f}) {
        const float angle=degree*3.14159265f/180;
        const float px=4734+fraction,py=4649+fraction;
        float tx=0,ty=0;
        Check(spellaim::Point(px,py,{std::cos(angle),0,std::sin(angle)},tx,ty),"valid hand aim produces a bounded target");
        for(int moveX:{-8,0,8})for(int moveY:{-8,0,8})
            Check(accepted(int(px)+moveX,int(py)+moveY,int(tx),int(ty)),
                  "all headings pass native validation with client/server movement margin");
    }
    std::puts("Spell aiming: native behavior, scoped targeting, movement-independent cast resolution and cell precision passed");
}
