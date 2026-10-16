#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
namespace framevr::placement {
constexpr uint16_t Back=0x20, Up=1, Down=2, Left=4, Right=8, Directions=15;
struct Slot {
    bool ready=false, consuming=false, held=false;
    uint16_t last=0, quarantine=0;
    uint64_t pressed=0, seen=0, pulseUntil=0;
    uint32_t epoch=0;
};
// Physical XInput only. No synthetic device and no gameplay button remapping.
struct Controls {
    std::array<Slot,4> slots{};
    int owner=-1;
    int heightCm=74;
    bool dirty=false;
    void Configure(float metres) {
        if(std::isfinite(metres)&&!dirty&&owner<0)
            heightCm=int(std::lround(std::clamp(metres,.2f,2.f)*100));
    }
    bool Pending() const {
        for(const auto& s:slots)if(s.consuming||s.quarantine||s.pulseUntil)return true;
        return false;
    }
    void Cancel(int index) {
        auto& s=slots[index];
        if(s.consuming)s.quarantine|=s.last&(Back|Directions);
        s.consuming=s.held=s.ready=false;s.pulseUntil=0;
        if(owner==index)owner=-1;
    }
    void CancelAll(){for(int i=0;i<4;++i)Cancel(i);}
    void Expire(uint64_t now) {
        for(int i=0;i<4;++i) {
            auto& s=slots[i];
            if((s.consuming||s.pulseUntil)&&(now<s.seen||now-s.seen>250))Cancel(i);
        }
    }
    uint16_t Poll(int index,bool connected,uint16_t buttons,bool allowed,uint32_t epoch,uint64_t now) {
        Expire(now);
        if(index<0||index>=4)return buttons;
        auto& s=slots[index];
        if(!connected){Cancel(index);s.last=0;s.quarantine=0;return buttons;}
        if(!allowed||(s.consuming&&epoch!=s.epoch))Cancel(index);
        s.seen=now;
        const bool back=(buttons&Back)!=0;
        s.quarantine&=buttons;
        if(s.quarantine&Back)s.quarantine|=buttons&Directions;
        if(s.quarantine) {
            // A canceled gesture must be released before either game or mod sees it again.
            if(s.consuming)Cancel(index);
            s.last=buttons;s.ready=!back;
            return buttons&~s.quarantine;
        }
        if(!allowed){s.last=buttons;s.ready=!back;return buttons;}
        if(!back) {
            if(s.consuming) {
                if(!s.held&&now-s.pressed<500)s.pulseUntil=now+50;
                if(owner==index)owner=-1;
                // Keep a D-pad held at release out of gameplay until released too.
                s.quarantine|=buttons&Directions;
                s.consuming=s.held=false;
            }
            s.ready=true;
        } else if(s.ready&&!s.consuming) {
            if(owner>=0&&owner!=index){s.quarantine=buttons&(Back|Directions);s.last=buttons;return buttons&~s.quarantine;}
            s.consuming=true;s.ready=false;s.pressed=now;s.epoch=epoch;
            s.pulseUntil=0;s.last=buttons; // directions already held are not a fresh step
        } else if(!s.consuming) {
            // Entering F4/reconnecting while Select is down cannot start a gesture.
            s.quarantine=buttons&(Back|Directions);s.last=buttons;
            return buttons&~s.quarantine;
        }
        uint16_t output=buttons;
        if(s.consuming) {
            if(!s.held&&now-s.pressed>=500) {
                if(owner>=0&&owner!=index){Cancel(index);s.last=buttons;return buttons&~s.quarantine;}
                owner=index;s.held=true;
            }
            if(s.held) {
                const uint16_t rise=(buttons&Directions)&~s.last;
                int delta=0;
                if((buttons&(Up|Down))!=(Up|Down))delta+=((rise&Up)?10:0)-((rise&Down)?10:0);
                if((buttons&(Right|Left))!=(Right|Left))delta+=((rise&Right)?1:0)-((rise&Left)?1:0);
                const int next=std::clamp(heightCm+delta,20,200);
                if(next!=heightCm){heightCm=next;dirty=true;}
            }
            output&=~(Back|Directions);
        }
        if(s.pulseUntil) {
            if(now<s.pulseUntil)output|=Back;
            else s.pulseUntil=0;
        }
        s.last=buttons;
        return output&~s.quarantine;
    }
};
}
