#pragma once
#include "skill_slots.h"
#include <cmath>

namespace framevr::skills {
// Right hand = 0. Input edges, ownership and cast arbitration are independent
// of native pointers. No queued casts: losing arbitration requires a release.
struct HandInput {
    bool active=false,occupied=false,click[2]{},face[2][2]{};
    float trigger[2]{},grip[2]{};
    uint64_t context=0,layout=0;
    unsigned independentSlots=0; // only bindings supported by the physical spell adapter
    double time=0;
};
struct HandOutput {int wheel=-1,slot=-1,hand=0;bool interact=false;int spells[2]{-1,-1};uint64_t activation[2]{};};
class HandControls {
    bool clickWas[2]{},held[2][3]{},blocked[2][3]{{true,true,true},{true,true,true}};
    bool primed=false;
    int wheel=-1,cast=-1,castHand=0,castButton=-1;
    bool castGrip=false;
    int spell[2]{-1,-1},spellButton[2]{-1,-1};
    bool spellGrip[2]{};
    uint64_t activation[2]{};
    uint64_t context=0,layout=0;
    double at=0;
public:
    void Suspend(){primed=false;Close();}
    void Close(){wheel=cast=spell[0]=spell[1]=-1;for(auto& h:blocked)for(auto& b:h)b=true;}
    int Wheel()const{return wheel;}
    HandOutput Step(const HandInput& in) {
        HandOutput out;
        const bool changed=!primed||in.context!=context||!std::isfinite(in.time)||in.time<=at||in.time-at>.1;
        const bool valid=in.active&&in.context&&!changed&&std::isfinite(in.trigger[0]+in.trigger[1]+in.grip[0]+in.grip[1]);
        if(!valid){Close();}
        const bool reassigned=layout!=in.layout;
        if(reassigned){const int prior=wheel;Close();wheel=prior;}
        layout=in.layout;
        primed=in.active&&std::isfinite(in.time);context=in.context;at=in.time;
        bool edge[2][3]{};bool toggled=false;
        for(int h=0;h<2;++h) {
            for(int b=0;b<3;++b) {
                const bool down=b==0?in.trigger[h]>(held[h][b]?.2f:.55f):in.face[h][b-1];
                edge[h][b]=down&&!held[h][b]&&!blocked[h][b];
                held[h][b]=down;
                if(!down&&valid&&!in.occupied)blocked[h][b]=false;
            }
            if(in.click[h]&&!clickWas[h]&&valid&&!in.occupied) {
                const int next=wheel==h?-1:h;Close();wheel=next;toggled=true;
            }
            clickWas[h]=in.click[h];
        }
        if(!valid||in.occupied||wheel>=0||toggled||reassigned) {
            cast=spell[0]=spell[1]=-1;for(auto& h:blocked)for(auto& b:h)b=true;
            out.wheel=wheel;return out;
        }
        if(cast>=0&&(!held[castHand][castButton]||
            (castButton>0&&(in.grip[castHand]>.6f)!=castGrip))) {
            if(held[castHand][castButton])blocked[castHand][castButton]=true;
            cast=-1;
        }
        for(int h=0;h<2;++h)if(spell[h]>=0&&(!held[h][spellButton[h]]||
            !(in.independentSlots&(1u<<spell[h]))||
            (spellButton[h]>0&&(in.grip[h]>.6f)!=spellGrip[h]))) {
            if(held[h][spellButton[h]])blocked[h][spellButton[h]]=true;
            spell[h]=-1;
        }
        // Physical spells have independent hand ownership. Native animated
        // actions still have one owner; losing presses require release.
        for(int h=0;h<2;++h)for(int b=0;b<3;++b)if(edge[h][b]) {
            const bool grip=in.grip[h]>.6f;
            const int slot=HandSlot(h,b+(b&&grip?2:0));
            if(cast<0&&spell[h]<0&&(in.independentSlots&(1u<<slot))) {
                spell[h]=slot;spellButton[h]=b;spellGrip[h]=grip;
                ++activation[h];
            }
            else if(cast<0&&spell[0]<0&&spell[1]<0){castHand=h;castButton=b;castGrip=grip;cast=slot;}
            else blocked[h][b]=true;
        }
        out.spells[0]=spell[0];out.spells[1]=spell[1];
        for(int h=0;h<2;++h)if(spell[h]>=0)out.activation[h]=activation[h];
        out.slot=cast;out.hand=castHand;return out;
    }
};
}
