#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

namespace projectiles {
inline bool SamePlayer(uint32_t localType,uint32_t localId,uint32_t ownerType,uint32_t ownerId) {
    return localType==0&&ownerType==0&&localId!=UINT32_MAX&&localId==ownerId;
}
inline int ScaledVelocity(int native,float multiplier) {
    if(native<=0||!std::isfinite(multiplier)||multiplier<1.f||multiplier>3.f)return native;
    const double value=double(native)*multiplier;
    return value<=std::numeric_limits<int>::max()?int(std::lround(value)):native;
}
}
