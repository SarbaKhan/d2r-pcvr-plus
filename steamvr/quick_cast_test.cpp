#include "quick_cast.h"
#include <cstdio>
#include <cstdlib>
#include <limits>
using namespace framevr::skills;
void Check(bool ok,const char* message){if(!ok){std::fprintf(stderr,"FAIL %s\n",message);std::exit(1);}}
struct Sim {
    QuickCast cast;bool allowed=true,second=false;float trigger=0;uint8_t faces=0,available=255;
    uint64_t session=10,epoch=1;uint32_t equipment=3;int bank=0;double now=0;
    Sim(){Step();}
    int Step(){now+=.02;cast.Capture(allowed,trigger,faces);return cast.Resolve(allowed,second,available,session,epoch,equipment,bank,now);}
};
int main(){
    const uint16_t buttons[]={0x2000,0x4000,0x8000,0x0200};
    for(int bank=0;bank<2;++bank)for(int key=0;key<4;++key){
        Sim s;s.trigger=1;s.second=bank;s.Step();s.faces=1u<<key;
        Check(s.Step()==key+bank*4,"all eight native slots reachable");
        Check(Buttons(key+bank*4)==buttons[key]&&Alternate(key+bank*4)==bool(bank),"cast routes to native button and modifier");
        Check(s.Step()==key+bank*4,"hold repeats through native cast cadence");
        s.faces=0;Check(s.Step()==-1,"release stops native skill immediately");
    }
    {
        Sim s;s.faces=4;s.Step();s.trigger=1;
        Check(s.Step()==-1,"held inventory key cannot become a cast on modifier press");
        s.faces=0;s.Step();s.faces=4;Check(s.Step()==2,"fresh face press casts");
        s.trigger=0;Check(s.Step()==-1&&s.cast.Suppress(2),"modifier release stops cast without opening inventory");
        s.faces=0;s.Step();Check(!s.cast.Suppress(2),"face release restores normal inventory key");
    }
    {
        Sim s;s.trigger=1;s.Step();s.faces=1;Check(s.Step()==0,"base shortcut");
        s.second=true;Check(s.Step()==-1&&s.Step()==-1,"grip cannot change a held cast to another skill");
        s.faces=0;s.Step();s.faces=1;Check(s.Step()==4,"fresh press uses alternate slot");
        s.faces=3;Check(s.Step()==-1,"simultaneous buttons stop ambiguous casting");
        s.faces=2;Check(s.Step()==-1,"releasing one simultaneous button does not fire the other");
    }
    for(int cause=0;cause<7;++cause){
        Sim s;s.trigger=1;s.Step();s.faces=8;Check(s.Step()==3,"casting before interruption");
        switch(cause){case 0:s.allowed=false;break;case 1:++s.session;break;case 2:++s.epoch;break;
            case 3:++s.equipment;break;case 4:s.bank=16;break;case 5:s.now+=.2;break;case 6:s.cast.Suspend();break;}
        Check(s.Step()==-1,"UI/tracking/context/hitch interruption cancels");s.allowed=true;
        Check(s.Step()==-1,"held shortcut cannot resume after interruption");
        s.faces=0;s.Step();s.faces=8;Check(s.Step()==3,"fresh press after interruption works");
    }
    {
        Sim s;s.trigger=1;s.Step();s.available=0;s.faces=1;
        Check(s.Step()==-1,"unassigned/unavailable/stale slots cannot cast");
        s.available=255;Check(s.Step()==-1,"snapshot becoming available does not activate held key");
        s.faces=0;s.Step();s.faces=1;Check(s.Step()==0,"fresh press after availability");
        s.trigger=std::numeric_limits<float>::quiet_NaN();Check(s.Step()==-1,"invalid modifier cancels");
    }
    Check(Buttons(-1)==0&&Buttons(Count)==0&&!Alternate(Count),"invalid slots do not map to controls");
    std::puts("Quick cast: eight slots, native mappings, ownership, release, bank changes and interruption rearm passed");
}
