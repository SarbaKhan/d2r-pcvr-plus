#include "skill_pad.h"
#include "skill_slots.h"
#include <cassert>
#include <cstdio>
using namespace framevr::skills;
int main() {
    SkillPad pad;
    assert(pad.Poll(Buttons(0),false,1,100)==Buttons(0));
    // Right Fire Bolt -> left Town Portal: same B button, distinct banks.
    assert(Buttons(0)==Buttons(4));
    assert(pad.Poll(Buttons(4),true,2,110)==0);
    assert(pad.Poll(Buttons(4),true,3,140)==0);
    assert(pad.Poll(Buttons(4),true,2,170)==0);
    assert(pad.Poll(Buttons(4),true,4,171)==Buttons(4));
    assert(pad.Poll(Buttons(0),false,5,180)==0);
    assert(pad.Poll(Buttons(0),false,6,231)==Buttons(0));
    // Releasing during preparation cannot queue a portal after release.
    pad.Reset();
    assert(pad.Poll(Buttons(4),true,1,100)==0);
    assert(pad.Poll(0,false,2,110)==0);
    assert(pad.Poll(0,false,3,170)==0);
    // Pauses and context resets require a new modifier-only report.
    assert(pad.Poll(Buttons(4),true,4,180)==0);
    assert(pad.Poll(Buttons(4),true,5,400)==0);
    assert(pad.Poll(Buttons(4),true,6,451)==Buttons(4));
    for(int slot=0;slot<Count;++slot) {
        pad.Reset();
        const auto first=pad.Poll(Buttons(slot),Alternate(slot),1,100);
        assert(first==(Alternate(slot)?0:Buttons(slot)));
        assert(pad.Poll(Buttons(slot),Alternate(slot),2,151)==Buttons(slot));
    }
    puts("Skill pad: modifier precedes button at consumer, hand changes release, stale and cancelled input cannot cast");
}
