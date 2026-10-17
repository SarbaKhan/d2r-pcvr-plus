#include "../vr/stereo_prop_physics.h"
#undef NDEBUG
#include <cassert>
#include <cstring>
#include <cstdio>
#include <map>
#include <vector>

int main() {
    using namespace stereoprops;
    std::map<uintptr_t,std::vector<uint8_t>> bytes;
    for (auto rva : {PrepareRva,SimulateRva,ThunkRva,CallsRva}) bytes[rva]={};
    // Install fingerprints into a synthetic image, then mutate every byte.
    Verified([&](uintptr_t rva,const uint8_t* sig,size_t n){bytes[rva]={sig,sig+n};return true;});
    const auto check=[&](uintptr_t rva,const uint8_t* sig,size_t n){
        const auto& b=bytes.at(rva);return b.size()==n&&!std::memcmp(b.data(),sig,n);
    };
    assert(Verified(check));
    for (auto& [rva,b] : bytes) for(auto& c : b) {c^=1;assert(!Verified(check));c^=1;}
    assert(!Verified([](uintptr_t,const uint8_t*,size_t){return false;}));
    const auto target=[](uintptr_t start,const uint8_t* sig){
        int32_t delta;std::memcpy(&delta,sig+1,4);return uintptr_t(intptr_t(start+5)+delta);
    };
    assert(target(CallsRva,CallsSig)==ThunkRva);
    assert(target(ThunkRva,ThunkSig)==PrepareRva);
    assert(target(CallsRva+12,CallsSig+12)==SimulateRva);

    // A synthetic native outer Step resets the render pose, prepares/integrates
    // physics, then reapplies it. Skipping the whole Step would show the reset
    // pose in the right eye. Only its two physics stages may be held.
    constexpr uintptr_t base=0x7ff600000000ULL;
    double physical=0,rendered=-1,elapsed=0;
    int prepared=0,integrated=0,posed=0;
    const auto step=[&](bool ready,bool paired,int eye,float dt,bool known=true){
        rendered=-1;
        Stage(Hold(ready,paired,eye,base+PrepareReturnRva+(known?0:1),base+PrepareReturnRva),[&]{++prepared;});
        Stage(Hold(ready,paired,eye,base+SimulateReturnRva+(known?0:1),base+SimulateReturnRva),[&]{++integrated;elapsed+=dt;physical+=dt*2;});
        rendered=physical;++posed;
    };
    for(int frame=0;frame<90;++frame){
        step(true,true,0,1.f/90);const double left=rendered;
        step(true,true,1,0.00001f);assert(rendered==left);
    }
    assert(prepared==90&&integrated==90&&posed==180);
    assert(elapsed>0.99999&&elapsed<1.00001);
    step(false,true,1,0.00001f); // partial installation: both stages remain native
    step(true,false,1,1.f/90);  // non-paired draw, even with a stale eye index
    step(true,true,1,1.f/90,false); // another native caller
    assert(prepared==93&&integrated==93&&posed==183);
    assert(!Hold(true,true,1,0,0));
    puts("Stereo prop physics: verified calls, coherent eye poses and native fallbacks passed");
}
