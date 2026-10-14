#include "../vr/projectile_rules.h"
#undef NDEBUG
#include <cassert>
#include <cstdio>
#include <limits>
int main() {
    using namespace projectiles;
    // Zero is a legitimate local player id. Monster ids may collide with it.
    assert(SamePlayer(0,0,0,0));
    assert(!SamePlayer(0,0,1,0));
    assert(!SamePlayer(0,7,0,8));
    assert(!SamePlayer(0,UINT32_MAX,0,UINT32_MAX));
    // Normal native arrow velocity is fixed-point. Retain a slowed baseline;
    // don't replace it with a magic hard-coded speed or accelerate stopped paths.
    assert(ScaledVelocity(24<<8,2)==48<<8);
    assert(ScaledVelocity(12<<8,2)==24<<8);
    assert(ScaledVelocity(0,2)==0);
    assert(ScaledVelocity(-1,2)==-1);
    assert(ScaledVelocity(6000,1)==6000);
    assert(ScaledVelocity(6000,std::numeric_limits<float>::quiet_NaN())==6000);
    assert(ScaledVelocity(6000,4)==6000);
    assert(ScaledVelocity(INT32_MAX,2)==INT32_MAX);
    puts("Projectile ownership/velocity tests passed");
}
