#pragma once
#include "frame_math.h"
#include <cstdint>
namespace framevr::mousecursor {
constexpr bool MenuVisible(int mode,bool menu,bool controller) {
    return menu&&controller&&(mode==1||mode==2||mode==3);
}
// DrawIconEx on black/white backgrounds supports both alpha cursors and
// classic AND masks. Recover straight RGBA for the OpenXR alpha layer.
inline void Pixel(const uint8_t* black,const uint8_t* white,uint8_t* rgba) {
    int transmission=0;
    for(int c=0;c<3;++c)transmission=std::max(transmission,int(white[c])-int(black[c]));
    const int alpha=255-std::clamp(transmission,0,255);
    for(int c=0;c<3;++c)rgba[c]=alpha?uint8_t(std::clamp((int(black[2-c])*255+alpha/2)/alpha,0,255)):0;
    rgba[3]=uint8_t(alpha);
}
inline math::Vec CenterOffset(float width,float height,float hotX,float hotY,float metresPerPixel) {
    return {(width*.5f-hotX)*metresPerPixel,(hotY-height*.5f)*metresPerPixel,0};
}
}
