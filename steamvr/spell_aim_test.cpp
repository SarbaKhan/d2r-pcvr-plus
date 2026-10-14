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
            Check(rounded.x*std::cos(angle)+rounded.z*std::sin(angle)>.9995f,"all horizontal aim angles remain accurate after native cell quantization");
        }
    }
    float x=5,y=7;
    Check(!spellaim::Point(0,0,{0,1,0},x,y)&&x==5&&y==7,"vertical aim cannot invent a horizontal direction");
    Check(!spellaim::Point(0,0,{NAN,0,1},x,y),"invalid direction is refused");
    std::puts("Spell aiming: native behavior classification, scoped targeting, fan direction and cell precision passed");
}
