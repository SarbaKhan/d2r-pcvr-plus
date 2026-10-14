#pragma once
#include "frame_math.h"
namespace framevr::potions {
// Original models are normalized once by prepare_potions.py: 18 cm tall,
// centered on the neck axis, +Y points out of the bottle's opening.
constexpr float Height=.18f;
inline math::Vec Center(math::Vec hand,math::Quat rotation) {
    return hand+math::Rotate(rotation,{0,.025f,0});
}
inline math::Vec Opening(math::Vec hand,math::Quat rotation) {
    return Center(hand,rotation)+math::Rotate(rotation,{0,Height*.5f,0});
}
inline bool AtMouth(math::Vec hand,math::Quat rotation,math::Vec head,math::Quat headRotation) {
    const auto mouth=head+math::Rotate(headRotation,{0,-.10f,-.08f});
    const auto difference=Opening(hand,rotation)-mouth;
    return math::Dot(difference,difference)<.105f*.105f&&math::Rotate(rotation,{0,1,0}).y<.25f;
}
}
