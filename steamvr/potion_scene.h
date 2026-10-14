#pragma once
#include "potion_pose.h"
#include "interactions.h"
namespace framevr::potions {
struct Instance {interaction::Pose pose;uint32_t code=0;bool empty=false,highlight=false;};
struct Scene {std::array<Instance,5> items{};unsigned count=0;};
inline Scene MakeScene(const interaction::Input& in,const interaction::Output& out) {
    Scene s;
    if(!in.active||!in.tracked[0]||!in.tracked[1]||!out.beltReady||!interaction::Fresh(in.snapshot,in.session,in.at))return s;
    for(int i=0;i<4;++i) {
        const auto& b=in.snapshot.belt[i];
        if(i==out.bottle||!b.known||!b.count||!b.Next().handle||!interaction::Valid(out.belt[i]))continue;
        s.items[s.count++]={out.belt[i],b.Next().code,false,i==out.nearBelt};
    }
    if(out.bottleHand>=0&&out.bottleHand<2&&out.bottleItem.handle) {
        auto p=in.hand[out.bottleHand];p.p=Center(p.p,p.q);
        s.items[s.count++]={p,out.bottleItem.code,out.bottleEmpty,false};
    }
    return s;
}
}
