#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
namespace framevr::placement {
constexpr uint16_t Back=0x20, Start=0x10, RB=0x200, Up=1, Down=2, Left=4, Right=8;
struct Slot {
    bool ready=false, pressing=false, entered=false, triggerQuarantine=false;
    uint16_t last=0, quarantine=0;
    uint64_t pressed=0, seen=0, pulseUntil=0;
    uint32_t epoch=0;
};
// Filter the real pad only. Select is never intercepted.
struct Controls {
    std::array<Slot,4> slots{};
    int owner=-1, heightCm=74, widthCm=150;
    bool dirty=false, moving=false, recenter=false;
    void Configure(float height,float width=1.5f) {
        if(!dirty&&owner<0) {
            if(std::isfinite(height))heightCm=int(std::lround(std::clamp(height,.2f,1.5f)*100));
            if(std::isfinite(width))widthCm=int(std::lround(std::clamp(width,.5f,3.f)*100));
        }
    }
    bool Pending() const {
        if(owner>=0)return true;
        for(const auto& s:slots)if(s.pressing||s.quarantine||s.triggerQuarantine||s.pulseUntil)return true;
        return false;
    }
    void Cancel(int i) {
        auto& s=slots[i];
        if(owner==i){s.quarantine|=s.last;s.triggerQuarantine=true;owner=-1;moving=false;recenter=false;}
        if(s.pressing)s.quarantine|=s.last&Start;
        s.pressing=s.entered=s.ready=false;s.pulseUntil=0;
    }
    void CancelAll(){for(int i=0;i<4;++i)Cancel(i);}
    void Expire(uint64_t now) {
        for(int i=0;i<4;++i)if((slots[i].pressing||owner==i||slots[i].pulseUntil)&&
            (now<slots[i].seen||now-slots[i].seen>250))Cancel(i);
    }
    uint16_t Poll(int i,bool connected,uint16_t buttons,bool allowed,uint32_t epoch,uint64_t now,
                  uint8_t lt=0,uint8_t rt=0) {
        Expire(now);if(i<0||i>=4)return buttons;
        auto& s=slots[i];
        if(!connected){Cancel(i);s={};return buttons;}
        if(!allowed||((s.pressing||owner==i)&&epoch!=s.epoch)){if(owner==i)s.last=buttons;Cancel(i);}
        s.seen=now;s.quarantine&=buttons;
        if(lt<=30&&rt<=30)s.triggerQuarantine=false;
        const bool start=(buttons&Start)!=0;
        const uint16_t rise=buttons&~s.last;
        if(!allowed||s.quarantine&Start){s.last=buttons;s.ready=!start;return buttons&~s.quarantine;}
        if(owner>=0) {
            if(owner!=i){s.last=buttons;return buttons&~s.quarantine;}
            if(start&&(rise&Start)&&!s.pressing){s.pressing=true;s.entered=false;s.pressed=now;}
            if(!start&&s.pressing) {
                const bool exit=!s.entered;s.pressing=false;s.entered=false;
                if(exit){s.last=buttons;Cancel(i);return 0;}
            }
            moving=rt>30;
            if(rise&RB)recenter=true;
            const int step=lt>30?1:10;
            int h=heightCm,w=widthCm;
            if((buttons&(Up|Down))!=(Up|Down))h+=((rise&Up)?step:0)-((rise&Down)?step:0);
            if((buttons&(Left|Right))!=(Left|Right))w+=((rise&Right)?10:0)-((rise&Left)?10:0);
            h=std::clamp(h,20,150);w=std::clamp(w,50,300);
            dirty|=h!=heightCm||w!=widthCm;heightCm=h;widthCm=w;
            s.last=buttons;return 0;
        }
        if(start&&!s.pressing) {
            if(s.ready){s.pressing=true;s.pressed=now;s.epoch=epoch;s.pulseUntil=0;}
            else s.quarantine|=Start;
        }
        if(s.pressing&&start&&now-s.pressed>=500){owner=i;s.entered=true;moving=false;}
        if(!start&&s.pressing){s.pressing=false;s.pulseUntil=now+50;}
        if(!start)s.ready=true;
        uint16_t out=buttons;
        if(s.pressing)out&=~Start;
        if(s.pulseUntil){if(now<s.pulseUntil)out|=Start;else s.pulseUntil=0;}
        s.last=buttons;return owner==i?0:out&~s.quarantine;
    }
    bool BlockAxes(int i) const {return i>=0&&i<4&&(owner==i||slots[i].triggerQuarantine);}
};
}
