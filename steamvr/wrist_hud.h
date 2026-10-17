#pragma once
#include <cstdint>

namespace framevr::wrist {
// Borrow a corner layout without writing the game's saved map preference.
// If the player changes it while borrowed, remember the new value to restore.
class MapLayoutLease {
    bool owned=false;
    int saved=0;
public:
    int Step(bool active,int current) {
        if(current<0||current>2){owned=false;return -1;}
        if(active){
            if(!owned||current!=1)saved=current;
            owned=true;return current==1?-1:1;
        }
        if(!owned)return -1;
        owned=false;return current==1&&saved!=1?saved:-1;
    }
};
// Cumulative XP exceeds INT_MAX at high levels. Keep the native unsigned
// values through subtraction and use double only for the displayed fraction.
struct Experience {
    uint32_t level=0,maximumLevel=0,current=0,start=0,next=0;
    bool known=false;
    bool operator==(const Experience&) const = default;
    bool Valid() const {
        return known&&level>=1&&level<=maximumLevel&&current>=start&&
            (level==maximumLevel||(next>start&&current<next));
    }
    bool Maximum() const {return Valid()&&level==maximumLevel;}
    double Progress() const {return !Valid()?0:Maximum()?1:double(current-start)/double(next-start);}
};

// The render thread renews a short lease; UI-thread code opens/closes only
// the native map it owns. Hysteresis bridges brief render/UI callback gaps.
class MapLease {
    uint64_t session=0,lastWanted=0;
    bool owned=false,wanted=false;
public:
    // -1: no native action, 0: restore closed map, 1: temporarily open map.
    int Step(uint64_t player,uint64_t now,bool request,bool menu,bool open) {
        const bool changed=player!=session;
        if(changed) {session=player;lastWanted=0;wanted=false;}
        if(menu||!player)wanted=false;
        if(player&&request&&!menu) {lastWanted=now;wanted=true;}
        const bool active=player&&!menu&&wanted&&now>=lastWanted&&now-lastWanted<750;
        if(!active||changed) {
            const bool close=owned&&open;owned=false;
            if(close)return 0;
        }
        if(active&&!open) {owned=true;return 1;}
        return -1;
    }
    bool Active(uint64_t now,bool menu) const {
        return session&&!menu&&wanted&&now>=lastWanted&&now-lastWanted<750;
    }
};
}
