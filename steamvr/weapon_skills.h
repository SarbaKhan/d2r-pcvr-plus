#pragma once
#include <cmath>
#include <cstdint>

namespace framevr::weaponskills {
inline constexpr int Smite=97,Strafe=26;
enum class Action { Native, Arrow, Volley, Shield };
inline Action Classify(int skill) {
    switch(skill) {
        case 6:case 7:case 11:case 12:case 16:case 21:case 22:case 27:case 31:return Action::Arrow;
        case Strafe:return Action::Volley;
        case Smite:return Action::Shield;
        default:return Action::Native;
    }
}
inline bool Bow(int skill) {return Classify(skill)==Action::Arrow||Classify(skill)==Action::Volley;}
inline const char* Hint(int skill,bool bow,bool shield) {
    if(bow&&Bow(skill))return skill==Strafe?"Release for volley":"Draw & release";
    if(shield&&skill==Smite)return "Shield swing / thrust";
    return "Point & trigger";
}
struct ContactContext {
    uint64_t session=0,epoch=0,assignment=0;
    uint32_t equipment=0;
    int slot=-1,owner=-1;
    bool operator==(const ContactContext&) const=default;
};
inline bool Current(const ContactContext& saved,const ContactContext& current,uint64_t at,uint64_t now,uint64_t maxAge=1500) {
    return saved.session&&saved.slot>=0&&saved.slot<8&&saved==current&&now>=at&&now-at<=maxAge;
}
// One brief native button press per physical release. Native attack state,
// skill availability, mana/ammunition and sequence timing stay authoritative.
class Pulse {
    int slot=-1,id=-1,owner=-1;
    uint64_t context=0;
    double started=0,until=0;
public:
    void Cancel(){*this=Pulse{};}
    bool Start(int nextSlot,int nextId,int nextOwner,uint64_t nextContext,double now) {
        if(nextSlot<0||nextSlot>=8||nextId<0||!std::isfinite(now)||now<until)return false;
        slot=nextSlot;id=nextId;owner=nextOwner;context=nextContext;started=now;until=now+.065;return true;
    }
    int Get(int currentSlot,int currentId,int currentOwner,uint64_t currentContext,double now) {
        if(!std::isfinite(now)||now<started||now>=until||currentSlot!=slot||currentId!=id||currentOwner!=owner||currentContext!=context) {
            Cancel();return -1;
        }
        return slot;
    }
};
}
