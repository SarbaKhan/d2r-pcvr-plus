// D2R 3.3.93787: client/server missile creation, path velocity, and the HD
// release-point handoff. Kept separate from posing the held bow.
#include <windows.h>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include "projectiles.h"
#include "projectile_rules.h"
#include "sigscan.h"
#include "memory.h"
#include "logging.h"
#include "combat.h"
#include "bow.h"

namespace gamestate {uint64_t LocalPlayer();}

namespace projectiles {
namespace {
constexpr uint64_t ClientRva=0x1B7760,ServerRva=0x5371A0,VelocityRva=0x342AE0;
constexpr uint64_t ReleaseRva=0x771A10,RemoveReleaseRva=0x70DC70,OwnerRva=0x09A7B0;
constexpr uint64_t ReleaseStorageRva=0x6E3CD0,RegistryRva=0x2677170;
constexpr uint8_t ClientSig[]={0x89,0x54,0x24,0x10,0x55,0x57,0x41,0x55,0x41,0x57,0x48,0x8d,0x6c,0x24,0xc1};
constexpr uint8_t ServerSig[]={0x40,0x55,0x57,0x41,0x56,0x48,0x8d,0x6c,0x24,0xc0,0x48,0x81,0xec,0x40,0x01,0x00,0x00};
constexpr uint8_t VelocitySig[]={0x48,0x85,0xc9,0x74,0x55,0x48,0x89,0x5c,0x24,0x08,0x57,0x48,0x83,0xec,0x20};
constexpr uint8_t ReleaseSig[]={0x40,0x55,0x53,0x56,0x57,0x41,0x55,0x41,0x56,0x41,0x57,0x48,0x8d,0xac,0x24,0x90,0xfd,0xff,0xff};
constexpr uint8_t RemoveSig[]={0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x74,0x24,0x18,0x57,0x48,0x83,0xec,0x20};
constexpr uint8_t OwnerSig[]={0x40,0x53,0x48,0x83,0xec,0x20,0xba,0x00,0x04,0x00,0x00,0x48,0x8b,0xd9,0xe8,0x3d,0x53,0x2b,0x00};
constexpr uint8_t StorageSig[]={0x48,0x89,0x5c,0x24,0x20,0x56,0x57,0x41,0x56,0x48,0x83,0xec,0x40,0x48,0x8b,0xf9,0xe8,0x9b,0x66,0x0b,0x00};

using ClientFn=uintptr_t(*)(uintptr_t,int);
using ServerFn=uintptr_t(*)(uintptr_t,uintptr_t);
using VelocityFn=void(*)(uintptr_t,int,uintptr_t,int);
using ReleaseFn=void(*)(uintptr_t,uintptr_t,uint32_t,uintptr_t);
ClientFn originalClient=nullptr;
ServerFn originalServer=nullptr;
VelocityFn originalVelocity=nullptr;
ReleaseFn originalRelease=nullptr;
std::atomic<bool> enabled{false},direct{true},speedHooksReady{false},allReady{false};
std::atomic<float> speed{2.f};
bool clientIn=false,serverIn=false,velocityIn=false,releaseIn=false;
std::atomic<unsigned> accelerated[2]{},released{},observed[2]{};
std::atomic<int> before[2]{},after[2]{};

template<class T> bool Read(uintptr_t p,T& value) {
    return p>0x10000&&framevr::ReadMemory(&value,reinterpret_cast<const void*>(p),sizeof value);
}
bool IsLocalOwner(uintptr_t owner) {
    uint32_t id=~0u,type=~0u,ownerId=~0u;
    return combat::LocalPlayerId(id)&&Read(owner,type)&&Read(owner+8,ownerId)&&SamePlayer(0,id,type,ownerId);
}
bool LocalArrowParams(uintptr_t params) {
    uint32_t missile=~0u;uintptr_t owner=0;
    return enabled.load()&&Read(params+0x20,missile)&&missile==0&&Read(params+8,owner)&&IsLocalOwner(owner);
}

// Scope is on the game's actual creation thread, on BOTH client and server.
// Nesting an unrelated projectile temporarily clears it; unrelated path calls
// never inherit a previous arrow. No game-owned table or request is edited.
struct Creation {int side=-1;float multiplier=1;uintptr_t changedPath=0;};
thread_local Creation creation;
struct Scope {
    Creation saved;
    Scope(uintptr_t params,int side):saved(creation) {
        creation={};
        if(speedHooksReady.load()&&LocalArrowParams(params)) {
            creation.side=side;creation.multiplier=speed.load();++observed[side];
        }
    }
    ~Scope(){creation=saved;}
};
uintptr_t Client(uintptr_t params,int flags) {Scope scope(params,0);return originalClient(params,flags);}
uintptr_t Server(uintptr_t game,uintptr_t params) {
    Scope scope(params,1);const auto unit=originalServer(game,params);
    if(unit)bowvr::Created(game,params,unit);
    return unit;
}
void Velocity(uintptr_t path,int native,uintptr_t source,int line) {
    int adjusted=native;
    // Native creation first clears velocity, then sets the final value. Only
    // the latter is scaled, once per path per creation; slow-missile and later
    // corrections outside creation retain their normal behavior.
    if(creation.side>=0&&native>0&&path!=creation.changedPath) {
        uintptr_t unit=0,back=0;uint32_t type=~0u,missile=~0u;
        if(Read(path+0x40,unit)&&Read(unit,type)&&type==3&&Read(unit+4,missile)&&missile==0&&Read(unit+0x38,back)&&back==path) {
            adjusted=ScaledVelocity(native,creation.multiplier);
            creation.changedPath=path;
            if(adjusted!=native) {const int side=creation.side;++accelerated[side];before[side]=native;after[side]=adjusted;}
        }
    }
    originalVelocity(path,adjusted,source,line);
}

bool EntityAlive(uintptr_t registry,uint32_t entity) {
    uintptr_t begin=0,end=0;uint32_t full=0;const auto index=entity&0xfffff;
    return index!=0xfffff&&Read(registry+0x30,begin)&&Read(registry+0x38,end)&&end>=begin&&
        (end-begin)/4<=0x100000&&index<(end-begin)/4&&Read(begin+index*4,full)&&full==entity;
}
bool HasComponent(uintptr_t storage,uint32_t entity) {
    uintptr_t begin=0,end=0,page=0,packed=0,packedEnd=0;uint32_t index=0,full=0;
    const auto pageIndex=(entity>>8)&0xfff;
    if(!Read(storage+8,begin)||!Read(storage+16,end)||end<begin||(end-begin)/8>4096||pageIndex>=(end-begin)/8||
       !Read(begin+pageIndex*8,page)||!Read(page+(entity&255)*4,index))return false;
    index&=0xfffff;
    return index!=0xfffff&&Read(storage+0x20,packed)&&Read(storage+0x28,packedEnd)&&packedEnd>=packed&&
        (packedEnd-packed)/4<=0x100000&&index<(packedEnd-packed)/4&&Read(packed+index*4,full)&&full==entity;
}
void Release(uintptr_t assets,uintptr_t definition,uint32_t entity,uintptr_t unit) {
    uint32_t type=~0u,missile=~0u;
    if(enabled.load()&&Read(unit,type)&&type==3&&Read(unit+4,missile)&&framevr::bowflight::BasicMissile(missile)) {
        const auto owner=reinterpret_cast<uintptr_t(*)(uintptr_t)>(d2rsig::Addr(OwnerRva))(unit);
        const auto registry=d2rsig::Addr(RegistryRva);
        if(EntityAlive(registry,entity)) {
            const auto storage=reinterpret_cast<uintptr_t(*)(uintptr_t)>(d2rsig::Addr(ReleaseStorageRva))(registry);
            const bool hasReleasePoint=HasComponent(storage,entity);
            // This callback also visits the unit root. Only the release mesh
            // may own the physical world transform; the root has no component.
            const bool physical=bowvr::Visual(entity,unit,hasReleasePoint);
            if(hasReleasePoint&&(physical||(missile==0&&direct.load()&&IsLocalOwner(owner)))) {
                // Use the native removal operation, on the same renderer
                // thread as initialization. The base arrow transform/heading
                // then follows its path immediately instead of blending from
                // the hero's original animated release bone for 0.5 seconds.
                reinterpret_cast<void(*)(uintptr_t,uint32_t)>(d2rsig::Addr(RemoveReleaseRva))(registry,entity);
                ++released;
                return;
            }
        }
    }
    originalRelease(assets,definition,entity,unit);
}
}
bool Ready(){return allReady.load();}
uintptr_t CreateVisual(uintptr_t params){return originalClient?Client(params,0):0;}
void Configure(bool motionControls,float multiplier,bool directRelease) {
    enabled=motionControls;speed=std::isfinite(multiplier)?std::clamp(multiplier,1.f,3.f):1.f;direct=directRelease;
}
void Install() {
    if(!enabled.load())return;
    if(!velocityIn)velocityIn=d2rsig::Hook(VelocityRva,VelocitySig,sizeof VelocitySig,reinterpret_cast<void*>(&Velocity),reinterpret_cast<void**>(&originalVelocity));
    if(!clientIn)clientIn=d2rsig::Hook(ClientRva,ClientSig,sizeof ClientSig,reinterpret_cast<void*>(&Client),reinterpret_cast<void**>(&originalClient));
    if(!serverIn)serverIn=d2rsig::Hook(ServerRva,ServerSig,sizeof ServerSig,reinterpret_cast<void*>(&Server),reinterpret_cast<void**>(&originalServer));
    if(velocityIn&&clientIn&&serverIn&&!speedHooksReady.exchange(true))
        framevr::logging::Write("Frame bow flight: client/server velocity hooks ready (local basic arrows only)");
    if(!releaseIn&&d2rsig::Check(OwnerRva,OwnerSig,sizeof OwnerSig)&&d2rsig::Check(RemoveReleaseRva,RemoveSig,sizeof RemoveSig)&&
       d2rsig::Check(ReleaseStorageRva,StorageSig,sizeof StorageSig)) {
        releaseIn=d2rsig::Hook(ReleaseRva,ReleaseSig,sizeof ReleaseSig,reinterpret_cast<void*>(&Release),reinterpret_cast<void**>(&originalRelease));
        if(releaseIn)framevr::logging::Write("Frame bow flight: native release-point handoff hook ready");
    }
    allReady=releaseIn&&speedHooksReady.load();
}
void Report() {
    const unsigned c=accelerated[0].exchange(0),s=accelerated[1].exchange(0),r=released.exchange(0);
    const unsigned oc=observed[0].exchange(0),os=observed[1].exchange(0);
    if(!c&&!s&&!r&&!oc&&!os)return;
    char line[256];snprintf(line,sizeof line,"Frame bow flight: created client/server %u/%u, accelerated %u/%u (%d->%d / %d->%d), direct handoffs %u",
        oc,os,c,s,before[0].load(),after[0].load(),before[1].load(),after[1].load(),r);
    framevr::logging::Write(line);
}
}
