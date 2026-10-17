#pragma once
#include <cstdint>

namespace framevr::interaction {
// One target per physical press (CalMac91, PR #70), including across native
// menus that bypass gameplay translation. Observe releases there as well.
class InteractPress {
    bool wasHeld=false,primed=false,blocked=false;
    uintptr_t latch=0;
    uint64_t session=0;
    uint32_t epoch=0;
public:
    struct Result {bool down=false,miss=false,lost=false;};
    Result Poll(bool held,uintptr_t target,bool hooked,bool active,uint64_t nextSession,uint32_t nextEpoch) {
        const bool changed=primed&&(session!=nextSession||epoch!=nextEpoch);
        primed=true;session=nextSession;epoch=nextEpoch;
        if(changed){latch=0;if(wasHeld&&held)blocked=true;}
        if(!active) {
            // A held button cannot carry an old interaction into another
            // context. A release observed in a menu rearms the next press.
            latch=0;wasHeld=held;blocked=held;return {};
        }
        Result out;
        if(!held){latch=0;blocked=false;}
        else if(!wasHeld){latch=target;out.miss=!latch&&hooked;}
        else if(latch&&target!=latch){latch=0;out.lost=true;}
        wasHeld=held;
        out.down=held&&!blocked&&(latch!=0||!hooked);
        return out;
    }
};
}
