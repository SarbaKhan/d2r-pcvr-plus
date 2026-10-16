#pragma once
namespace framevr::dioramamr {
// Optional alpha composition, never a prerequisite for VR session creation.
constexpr bool Enabled(int mode,bool requested,bool supported,bool alphaTexture,bool shader,bool blocked) {
    return mode==3&&requested&&supported&&alphaTexture&&shader&&!blocked;
}
}
