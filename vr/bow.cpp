// Physical basic arrows, D2R 3.3.93787. Native calls are confined to their
// owning game/UI threads. Rendering consumes immutable release/flight data.
#include <windows.h>
#include <D2RLPlugin/api.h>
#include <array>
#include <atomic>
#include <mutex>
#include <cstdio>
#include <cstring>
#include "bow.h"
#include "bow_signatures.h"
#include "melee.h"
#include "combat.h"
#include "projectiles.h"
#include "sigscan.h"
#include "memory.h"
#include "logging.h"
#include "../shared/d2r_vr_state.h"
#include "../steamvr/transport.h"
#include "../steamvr/callback_ticket.h"
namespace gamestate {uint64_t LocalPlayer();bool MenuOpen();uint32_t HandsKey();uint32_t WeaponSet();uint32_t WeaponType();}
namespace bowvr {
namespace {
using namespace framevr::bowflight;
using namespace native;
const D2RL::PluginContext* context=nullptr;
const D2RL::ThreadService* threads=nullptr;
std::atomic<bool> enabled{false},ready{false};
using ShootFn=uintptr_t(*)(uintptr_t,int,uintptr_t,int,int,int,int,int,int,int,int);
using CollisionFn=int(*)(uintptr_t,uintptr_t);
using ClientTickFn=void(*)(uintptr_t,float);
using ClientKillFn=void(*)(uintptr_t);
using TransformFn=uintptr_t(*)(uintptr_t,uint32_t,uintptr_t);
using MissileSoundFn=int(*)(int,uintptr_t,int,int,bool);
ShootFn originalShoot=nullptr;CollisionFn originalCollision=nullptr;
ClientTickFn originalClientTick=nullptr;TransformFn originalTransform=nullptr;
ClientKillFn originalClientKill=nullptr;
MissileSoundFn originalMissileSound=nullptr;
bool soundIn=false;
bool shootIn=false,collisionIn=false,clientIn=false,killIn=false,transformIn=false,checked=false;
std::mutex mutex;
struct Arrow {
    Flight flight{};
    uintptr_t game=0,server=0,client=0,room=0;
    uint32_t serverId=0,clientId=0;
    VisualBinding visualBinding{};
    bool visualDone=false,creating=false;
    uint64_t renderAt=0;
    std::array<float,16> rendered{},previous{};
};
std::array<Arrow,32> arrows{};
struct Preview {
    Shot pose{};
    bool active=false,visible=false;
    uintptr_t client=0;
    uint32_t clientId=0;
    VisualBinding binding{};
    uint64_t renderAt=0;
    std::array<float,16> rendered{},previous{};
};
Preview preview;
std::atomic<bool> previewClient{false};
std::atomic<uint64_t> previewReadyAt{0};
uint64_t previewAttemptAt=0;
Gate current{};uintptr_t clientHero=0;
Once once;
std::array<Shot,4> pending{};size_t pendingCount=0;
framevr::CallbackTicket gameCallback,uiCallback;
uint64_t maintenanceAt=0;
std::atomic<unsigned> renderCount{0},serverCount{0},clientCount{0},submitted{0},spawned{0},visuals{0},stale{0},refused{0},contacts{0},walls{0},expired{0};
std::atomic<unsigned> lastLatency{0},lastSpeedCm{0};
std::atomic<unsigned> boundMeshes{0},ignoredEntities{0};
std::atomic<unsigned> positionMismatch{0};
std::atomic<unsigned> previewCreated{0},previewRefused{0},uiRecovered{0};
std::atomic<unsigned> gameRecovered{0};
struct Launch {Shot shot{};uintptr_t game=0,player=0;int slot=-1;bool created=false;};
thread_local Launch launch;
thread_local int creatingVisual=-1;
thread_local uint64_t visualSession=0,visualSequence=0;
thread_local uint64_t previewCreatingSession=0;
int MissileSound(int sound,uintptr_t unit,int a,int b,bool c) {
    // The nocked model is not a fired projectile. Suppress only its creation
    // scope's travel sound; genuine releases and other game audio pass through.
    if(previewCreatingSession)return 0;
    return originalMissileSound(sound,unit,a,b,c);
}
template<class T> bool Read(uintptr_t p,T& out) {
    return p>0x10000&&framevr::ReadMemory(&out,reinterpret_cast<const void*>(p),sizeof out);
}
template<class T> void Param(std::array<uint8_t,0x98>& p,size_t offset,T value) {
    std::memcpy(p.data()+offset,&value,sizeof value);
}
bool Position(uintptr_t unit,Vec& out) {
    uintptr_t path=0;uint16_t p[4]{};
    if(!Read(unit+0x38,path)||!Read(path,p))return false;
    out={2*(p[1]+p[0]/65536.f),0,2*(p[3]+p[2]/65536.f)};return true;
}
bool Live(const Shot& s) {
    Gate gate;{std::lock_guard lock(mutex);gate=current;}
    return enabled.load()&&ready.load()&&Allowed(s,gate,GetTickCount64())&&
        s.session==gamestate::LocalPlayer()&&!gamestate::MenuOpen()&&
        s.equipment==(gamestate::HandsKey()^(gamestate::WeaponSet()<<30));
}
bool Same(const Arrow& a,const Arrow& b) {
    return a.flight.shot.session==b.flight.shot.session&&a.flight.shot.sequence==b.flight.shot.sequence;
}
void Recount();
void Store(size_t slot,const Arrow& a) {
    std::lock_guard lock(mutex);
    if(Same(arrows[slot],a)) {
        // UI may have attached the client/entity while the game was stepping.
        arrows[slot].flight=a.flight;arrows[slot].room=a.room;Recount();
    }
}
void Recount() { // mutex held
    unsigned r=preview.binding.Bound()?1:0,s=0,c=0;
    for(const auto& a:arrows){if(a.visualBinding.Bound())++r;if(a.flight.alive)++s;if(a.client)++c;}
    renderCount=r;serverCount=s;clientCount=c;
    previewClient=preview.client!=0;
}
int End(size_t slot,Arrow& a,uintptr_t game,uintptr_t unit) {
    a.flight.alive=false;a.flight.updatedAt=GetTickCount64();Store(slot,a);
    reinterpret_cast<int(*)(uintptr_t,uintptr_t,uintptr_t,int)>(d2rsig::Addr(HitRva))(game,unit,0,1);
    return 2;
}
bool Body(uintptr_t unit,uint32_t classId,const Shot& shot,Vec& bottom,Vec& top,float& radius) {
    uint8_t bank=0,fields[16]{};Vec p{};
    if(!Position(unit,p)||!Read(unit+0x1bd,bank)||bank>3||classId>4095)return false;
    const auto row=reinterpret_cast<uintptr_t(*)(uint8_t,int)>(d2rsig::Addr(StatsRva))(bank,int(classId));
    if(!Read(row,fields)||fields[8]<1||fields[8]>8||fields[9]<1||fields[9]>8)return false;
    const float upm=shot.unitsPerMetre,height=std::clamp((fields[13]?fields[13]:72)*.1f/upm,.4f,4.f);
    radius=std::min(std::max(.12f,std::max(fields[8],fields[9])*.7f/upm),height*.45f)+.025f;
    bottom=RenderGround(shot,p)+Vec{0,radius,0};top=bottom;top.y=shot.player.y+height-radius;
    return true;
}
int Collision(uintptr_t game,uintptr_t unit) {
    if(!serverCount.load(std::memory_order_relaxed))return originalCollision(game,unit);
    uint32_t header[3]{};Arrow a;size_t slot=arrows.size();
    if(!Read(unit,header))return originalCollision(game,unit);
    {
        std::lock_guard lock(mutex);
        for(size_t i=0;i<arrows.size();++i)if(arrows[i].server==unit&&arrows[i].serverId==header[2]&&
            arrows[i].game==game&&arrows[i].flight.shot.session==current.session){slot=i;a=arrows[i];break;}
    }
    if(slot==arrows.size())return originalCollision(game,unit);
    // Already-fired arrows survive menu/weapon/head movement. Only a session
    // change or disabling motion combat retires them, with no delayed attack.
    if(!enabled.load()||!a.flight.alive||a.flight.shot.session!=gamestate::LocalPlayer())return End(slot,a,game,unit);
    uintptr_t localGame=0,player=0;
    if(!melee::LocalServer(localGame,player)||localGame!=game)return End(slot,a,game,unit);
    combat::ObserveLocalPlayer(player);
    const int remaining=reinterpret_cast<int(*)(uintptr_t)>(d2rsig::Addr(FrameGetRva))(unit)-1;
    reinterpret_cast<void(*)(uintptr_t,int)>(d2rsig::Addr(FrameSetRva))(unit,remaining);
    if(remaining<=0){++expired;return End(slot,a,game,unit);}
    uintptr_t path=0;int velocity=0;
    if(!Read(unit+0x38,path)||!Read(path+0xa0,velocity))return End(slot,a,game,unit);
    a.flight.speed=Speed(velocity,a.flight.shot.draw);
    const float distance=a.flight.distance,step=a.flight.speed*.04f;
    const Vec from=distance==0?a.flight.shot.origin:Tip(a.flight.shot,distance),to=Tip(a.flight.shot,distance+step);
    const float length=ProjectileLength(a.flight.shot);
    const float swept=step+(distance==0?length:0);
    const auto advance=[&](float fraction){return std::max(0.f,distance+swept*fraction-(distance==0?length:0));};
    // The proxy's room is owned by the native unit. Reacquire it every tick;
    // a previous traversal's adjacent room may have unloaded in the meantime.
    a.room=reinterpret_cast<uintptr_t(*)(uintptr_t)>(d2rsig::Addr(RoomRva))(unit);
    // Native missile barriers, never monster occupancy: bodies are tested in
    // 3D below. Closed/unloaded room cells are conservative solid barriers.
    float stop=WallEntry(GridPoint(a.flight.shot,from),GridPoint(a.flight.shot,to),a.flight.shot.unitsPerMetre,[&](int x,int z) {
        if(x<1||z<1||x>65534||z>65534||!a.room)return true;
        const auto room=reinterpret_cast<uintptr_t(*)(uintptr_t,int,int)>(d2rsig::Addr(RoomPointRva))(a.room,x,z);
        if(!room)return true;
        a.room=room;
        return reinterpret_cast<uint16_t(*)(uintptr_t,int,int,int,int)>(d2rsig::Addr(MaskRva))(room,x,z,1,0x804)!=0;
    });
    // Until terrain-height queries are available, use the launch floor. Walls
    // extend vertically; this deliberately does not permit shooting over them.
    const float floor=a.flight.shot.player.y+.015f;
    if(from.y<=floor)stop=0;
    else if(to.y<=floor)stop=std::min(stop,(from.y-floor)/(from.y-to.y));
    struct Touch {float fraction;uintptr_t unit;uint32_t id;};
    std::array<Touch,64> touches{};size_t n=0;
    uintptr_t buckets[128]{};unsigned visited=0;
    if(!Read(game+0x4c6*8,buckets))return End(slot,a,game,unit);
    for(auto target:buckets)for(unsigned chain=0;target&&chain<256&&visited++<4096;++chain) {
        uintptr_t next=0;uint32_t h[4]{};
        if(!Read(target+0x158,next)||!Read(target,h))break;
        if(h[0]==1&&h[3]!=0&&h[3]!=12&&!a.flight.Seen(h[2])) {
            Vec bottom{},top{};float radius=0,fraction=0;
            Vec position{};
            // The largest capsule radius below is <2m. Reject distant units
            // before any monster-table lookup or detailed intersection work.
            const bool positioned=Position(target,position);
            const auto ground=RenderGround(a.flight.shot,position);
            const bool nearby=positioned&&ground.x>=std::min(from.x,to.x)-2&&
                ground.x<=std::max(from.x,to.x)+2&&ground.z>=std::min(from.z,to.z)-2&&ground.z<=std::max(from.z,to.z)+2;
            if(nearby&&Body(target,h[1],a.flight.shot,bottom,top,radius)&&CapsuleEntry(from,to,bottom,top,radius,fraction)&&
               fraction<stop&&reinterpret_cast<int(*)(uintptr_t,uintptr_t,uintptr_t)>(d2rsig::Addr(RelationRva))(game,player,target)) {
                const Touch touch{fraction,target,h[2]};
                if(n<touches.size())touches[n++]=touch;
                else {
                    auto worst=std::max_element(touches.begin(),touches.end(),[](const Touch& x,const Touch& y){return x.fraction<y.fraction;});
                    if(fraction<worst->fraction)*worst=touch;
                }
            }
        }
        target=next;
    }
    std::sort(touches.begin(),touches.begin()+n,[](const Touch& x,const Touch& y){return x.fraction<y.fraction;});
    for(size_t i=0;i<n;++i) {
        const auto& t=touches[i];
        uint32_t mode=0;
        // A preceding hit/proc can kill or remove another candidate.
        if(melee::ServerUnit(game,1,t.id)!=t.unit||!Read(t.unit+0xc,mode)||mode==0||mode==12)continue;
        if(!a.flight.Remember(t.id))return End(slot,a,game,unit);
        // Native missile owns weapon damage, costs, criticals, leech, procs,
        // immunities and pierce. It does not get a second ground-grid hit.
        const int result=reinterpret_cast<int(*)(uintptr_t,uintptr_t,uintptr_t,int)>(d2rsig::Addr(HitRva))(game,unit,t.unit,0);
        ++contacts;framevr::MeleeImpact();
        if(result==2) {
            a.flight.distance=advance(t.fraction);a.flight.alive=false;
            a.flight.updatedAt=GetTickCount64();Store(slot,a);return 2;
        }
    }
    a.flight.distance=advance(stop);a.flight.updatedAt=GetTickCount64();
    if(stop<1){++walls;return End(slot,a,game,unit);}
    Store(slot,a);return 1;
}
uintptr_t Shoot(uintptr_t game,int missile,uintptr_t player,int skill,int level,int extra,
                int offsetX,int offsetY,int targetX,int targetY,int ammo) {
    if(launch.slot>=0&&launch.game==game&&launch.player==player) {
        if(missile!=int(launch.shot.kind))return 0; // before native ammo use
        Vec p{};if(!Position(player,p))return 0;
        const auto& shot=launch.shot;
        const Vec tip=GridPoint(shot,Tip(shot,0))*shot.unitsPerMetre;
        const int x=int(tip.x*.5f),y=int(tip.z*.5f);
        offsetX=x-int(p.x*.5f);offsetY=y-int(p.z*.5f);
        const Vec horizontal=framevr::math::Normalized(Vec{shot.direction.x,0,shot.direction.z});
        // The native path is only a damage/lifetime owner. Actual flight uses
        // the complete frozen vector, including vertical shots.
        targetX=x+int(horizontal.x*60);targetY=y+int(horizontal.z*60);
        if(targetX==x&&targetY==y)++targetY;
        if(x<1||y<1||targetX<1||targetY<1||x>65534||y>65534||targetX>65534||targetY>65534)return 0;
    }
    return originalShoot(game,missile,player,skill,level,extra,offsetX,offsetY,targetX,targetY,ammo);
}
void __cdecl Apply(const D2RL::PluginContext*,void* token) noexcept {
    std::array<Shot,4> batch;size_t n=0;
    {std::lock_guard lock(mutex);
     if(!gameCallback.Claim(reinterpret_cast<uintptr_t>(token)))return;
     n=pendingCount;std::copy_n(pending.begin(),n,batch.begin());pendingCount=0;}
    // Room unloading/native removal can bypass the missile update hook. Retire
    // those records by authoritative id lookup, never by a wall-clock timeout
    // which would confuse an ordinary paused game with a destroyed projectile.
    uintptr_t checkGame=0,checkPlayer=0;
    if(melee::LocalServer(checkGame,checkPlayer))for(size_t j=0;j<arrows.size();++j) {
        Arrow a;{std::lock_guard lock(mutex);a=arrows[j];}
        if(a.flight.alive&&a.game==checkGame&&a.flight.shot.session==gamestate::LocalPlayer()&&
           melee::ServerUnit(checkGame,3,a.serverId)!=a.server) {
            a.flight.alive=false;a.flight.updatedAt=GetTickCount64();Store(j,a);
        }
    }
    for(size_t i=0;i<n;++i) {
        const auto& shot=batch[i];
        if(!Live(shot)){++stale;continue;}
        uintptr_t game=0,player=0;Vec p{};uint32_t mode=0;
        if(!melee::LocalServer(game,player)||!Read(player+0xc,mode)||mode==0||mode==17||mode==5||mode==6||
           !Position(player,p)) {++refused;continue;}
        if(!framevr::coordinates::MatchesPlayer(p,shot.player,shot.unitsPerMetre)) {
            ++refused;++positionMismatch;continue;
        }
        int level=1;
        const bool javelin=shot.kind==Kind::Javelin;
        if(javelin?gamestate::WeaponType()!=D2RVR_TYPE_JAVELIN:
           (!reinterpret_cast<int(*)(uintptr_t)>(d2rsig::Addr(IsBowRva))(player)||
            reinterpret_cast<int(*)(uintptr_t,int*)>(d2rsig::Addr(MissileTypeRva))(player,&level)!=0)) {++refused;continue;}
        int slot=-1;
        {
            std::lock_guard lock(mutex);
            for(size_t j=0;j<arrows.size();++j)if(!arrows[j].flight.alive&&!arrows[j].client&&!arrows[j].creating){slot=int(j);arrows[j]={};arrows[j].flight.shot=shot;break;}
        }
        if(slot<0){++refused;continue;}
        combat::ObserveLocalPlayer(player);
        const auto saved=launch;launch={shot,game,player,slot,false};
        // Native basic attack -> Shoot checks and consumes arrows once. There
        // is deliberately no controller A press or second client-side attack.
        reinterpret_cast<int(*)(uintptr_t,uintptr_t,int,int)>(d2rsig::Addr(javelin?ThrowRva:AttackRva))(game,player,javelin?2:0,1);
        if(!launch.created)++refused;
        launch=saved;
    }
}
void VisualParams(const Shot& shot,float distance,uintptr_t hero,std::array<uint8_t,0x98>& params) {
    const auto tip=GridPoint(shot,Tip(shot,distance))*shot.unitsPerMetre;
    const int x=int(tip.x*.5f),y=int(tip.z*.5f);
    const auto d=framevr::math::Normalized(Vec{shot.direction.x,0,shot.direction.z});
    int tx=x+int(d.x*60),ty=y+int(d.z*60);if(tx==x&&ty==y)++ty;
    Param(params,0,uint32_t(0x21));Param(params,8,hero);Param(params,0x10,hero);
    Param(params,0x20,uint32_t(shot.kind));
    Param(params,0x24,x);Param(params,0x28,y);Param(params,0x2c,tx);Param(params,0x30,ty);
    Param(params,0x40,int(1));
}
void PresentPreview(uintptr_t hero,uint64_t session) {
    Preview snapshot;
    {std::lock_guard lock(mutex);snapshot=preview;}
    if(snapshot.client) {
        // Keep one silent model through skill/weapon/menu changes. Repeated
        // allocation can replay TravelSound and race asynchronous mesh loading.
        if(enabled.load()&&snapshot.pose.session==session)return;
        uint32_t header[3]{};
        if(Read(snapshot.client,header)&&header[0]==3&&header[1]==0&&header[2]==snapshot.clientId)
            originalClientKill(snapshot.client);
        std::lock_guard lock(mutex);
        if(preview.client==snapshot.client){preview.client=0;preview.binding.Clear();previewReadyAt=0;Recount();}
        return;
    }
    if(!snapshot.active||!hero||!session||snapshot.pose.session!=session)return;
    // Client visual only: no server missile, attack, ammo or damage request.
    std::array<uint8_t,0x98> params{};VisualParams(snapshot.pose,0,hero,params);
    const auto saved=previewCreatingSession;previewCreatingSession=session;
    const auto client=projectiles::CreateVisual(reinterpret_cast<uintptr_t>(params.data()));
    previewCreatingSession=saved;
    uint32_t header[3]{};
    const bool valid=client&&Read(client,header)&&header[0]==3&&header[1]==0;
    bool orphan=false;
    {
        std::lock_guard lock(mutex);
        if(valid&&current.session==session&&preview.active&&(!preview.client||preview.client==client)) {
            preview.client=client;preview.clientId=header[2];++previewCreated;
        } else {
            orphan=valid;++previewRefused;
            if(preview.client==client){preview.client=0;preview.binding.Clear();}
        }
        Recount();
    }
    if(orphan)originalClientKill(client);
}
void __cdecl Present(const D2RL::PluginContext*,void* token) noexcept {
    uintptr_t hero=0;uint64_t session=0;
    {std::lock_guard lock(mutex);
     if(!uiCallback.Claim(reinterpret_cast<uintptr_t>(token)))return;
     hero=clientHero;session=current.session;}
    uint32_t h[3]{},id=0;
    if(!ready.load()||!session||session!=gamestate::LocalPlayer())return;
    const bool validHero=combat::LocalPlayerId(id)&&Read(hero,h)&&h[0]==0&&h[2]==id;
    PresentPreview(validHero?hero:0,session);
    if(!enabled.load()||!validHero)return;
    for(size_t i=0;i<arrows.size();++i) {
        Arrow a;
        {
            std::lock_guard lock(mutex);a=arrows[i];
            if(!a.flight.alive||a.flight.shot.session!=session||a.visualDone)continue;
            arrows[i].visualDone=true;arrows[i].creating=true;
        }
        // Only values cross game -> UI. No server owner/path/target pointer is
        // supplied to the client allocator, and its id is allocated normally.
        std::array<uint8_t,0x98> params{};
        VisualParams(a.flight.shot,a.flight.distance,hero,params);
        const auto saved=creatingVisual;const auto savedSession=visualSession,savedSequence=visualSequence;
        creatingVisual=int(i);visualSession=a.flight.shot.session;visualSequence=a.flight.shot.sequence;
        const auto client=projectiles::CreateVisual(reinterpret_cast<uintptr_t>(params.data()));
        creatingVisual=saved;visualSession=savedSession;visualSequence=savedSequence;
        uint32_t header[3]{};
        const bool validClient=client&&Read(client,header)&&header[0]==3&&header[1]==uint32_t(a.flight.shot.kind);
        bool orphan=false;
        {
            std::lock_guard lock(mutex);
            if(Same(arrows[i],a)) {
                arrows[i].creating=false;
                if(validClient){arrows[i].client=client;arrows[i].clientId=header[2];++visuals;Recount();}
                else ++refused;
            } else orphan=validClient;
        }
        if(orphan)originalClientKill(client);
    }
}
void ClientTick(uintptr_t unit,float dt) {
    if(!clientCount.load(std::memory_order_relaxed)&&!previewClient.load(std::memory_order_relaxed)){originalClientTick(unit,dt);return;}
    uint32_t header[3]{};size_t slot=arrows.size();bool alive=false;
    bool held=false;
    if(Read(unit,header)&&header[0]==3&&BasicMissile(header[1])) {
        std::lock_guard lock(mutex);
        if(preview.client==unit&&preview.clientId==header[2]) {
            held=true;alive=preview.pose.session==current.session&&enabled.load();
            if(!alive){preview.client=0;preview.binding.Clear();previewReadyAt=0;Recount();}
        }
        for(size_t i=0;i<arrows.size();++i)if(arrows[i].client==unit&&arrows[i].clientId==header[2]) {
            slot=i;alive=arrows[i].flight.alive&&arrows[i].flight.shot.session==current.session&&enabled.load();
            if(!alive){arrows[i].client=0;arrows[i].visualBinding.Clear();Recount();}break;
        }
    }
    if(held){if(!alive)originalClientKill(unit);return;}
    if(slot==arrows.size()){originalClientTick(unit,dt);return;}
    if(!alive)originalClientKill(unit);
    // Server owns lifetime/collision; the HD transform owns full 3D movement.
    // Running native client prediction here would resurrect flat-grid hits.
}
void ClientKill(uintptr_t unit) {
    if(!clientCount.load(std::memory_order_relaxed)&&!previewClient.load(std::memory_order_relaxed)){originalClientKill(unit);return;}
    {
        std::lock_guard lock(mutex);
        if(preview.client==unit){preview.client=0;preview.binding.Clear();previewReadyAt=0;}
        for(auto& a:arrows)if(a.client==unit){a.client=0;a.visualBinding.Clear();}
        Recount();
    }
    originalClientKill(unit);
}
uintptr_t Transform(uintptr_t out,uint32_t entity,uintptr_t component) {
    const auto result=originalTransform(out,entity,component);
    if(!renderCount.load(std::memory_order_relaxed))return result;
    std::array<float,16> matrix{},previous{};bool found=false,held=false,heldActive=false;
    const uint64_t at=framevr::BowRenderTime();
    {
        std::lock_guard lock(mutex);
        if(preview.binding.Matches(entity)&&preview.pose.session==current.session) {
            if(preview.renderAt!=at||preview.rendered[15]!=1) {
                preview.previous=preview.rendered;
                PreviewMatrix(preview.pose,preview.active&&preview.visible,preview.rendered.data());
                if(preview.previous[15]!=1)preview.previous=preview.rendered;
                preview.renderAt=at;
            }
            matrix=preview.rendered;previous=preview.previous;found=held=true;heldActive=preview.active;
        }
        for(auto& a:arrows)if(!held&&a.visualBinding.Matches(entity)&&a.flight.shot.session==current.session) {
            if(a.renderAt!=at||a.rendered[15]!=1) {
                a.previous=a.rendered;ArrowMatrix(a.flight,at,a.rendered.data());
                if(a.previous[15]!=1)a.previous=a.rendered;
                a.renderAt=at;
            }
            matrix=a.rendered;previous=a.previous;found=true;break;
        }
    }
    if(!found)return result;
    // Exact full entity generation, not its reusable 20-bit index. Both the
    // caller's result and the component cache need the same world transform.
    const bool written=framevr::WriteMemory(reinterpret_cast<void*>(out),matrix.data(),sizeof matrix)&&
        framevr::WriteMemory(reinterpret_cast<void*>(component+0x40),matrix.data(),sizeof matrix);
    framevr::WriteMemory(reinterpret_cast<void*>(component+0x80),previous.data(),sizeof previous);
    if(held&&heldActive&&written)previewReadyAt=GetTickCount64();
    return result;
}
}
void Init(const D2RL::PluginContext* ctx) {
    context=ctx;
    if(ctx->QueryService(&threads)!=D2RL::ServiceQueryResult::Success||
       !D2RL::HasThreadServiceField(threads,D2RL::ThreadServiceRequiredSize))threads=nullptr;
}
void Configure(bool on){enabled=on;}
bool PreviewReady() {
    const auto at=previewReadyAt.load();
    return enabled.load()&&at&&GetTickCount64()-at<150;
}
void Update(const Gate& gate,uintptr_t hero) {
    bool visible=false;
    Shot pose;const bool held=gate.active&&enabled.load()&&framevr::BowPreviewPose(pose,visible)&&Allowed(pose,gate,GetTickCount64());
    uintptr_t uiToken=0,gameToken=0;
    {
        std::lock_guard lock(mutex);
        if(current.session!=gate.session) {
            arrows={};preview={};previewReadyAt=0;previewAttemptAt=0;
            pendingCount=0;gameCallback.Cancel();uiCallback.Cancel();Recount();
        }
        current=gate;clientHero=hero;
        preview.active=held;preview.visible=held&&visible;
        if(held)preview.pose=pose;else previewReadyAt=0;
        const auto now=GetTickCount64();
        if(gameCallback.Expired(now,500)){gameCallback.Cancel();pendingCount=0;++gameRecovered;}
        if(uiCallback.Expired(now,500)){uiCallback.Cancel();++uiRecovered;}
        const bool needsPreview=(!enabled.load()&&preview.client)||(held&&visible&&!preview.client&&now-previewAttemptAt>=500);
        bool needsVisual=false;
        if(ready.load())for(const auto& a:arrows)if(a.flight.alive&&!a.visualDone){needsVisual=true;break;}
        if(ready.load()&&(needsPreview||needsVisual)) {
            uiToken=uiCallback.Start(now);
            if(uiToken&&needsPreview)previewAttemptAt=now;
        }
        if(ready.load()&&now-maintenanceAt>=100)
            for(const auto& a:arrows)if(a.flight.alive){gameToken=gameCallback.Start(now);if(gameToken)maintenanceAt=now;break;}
    }
    if(uiToken&&threads->runOnUiThread(context,&Present,reinterpret_cast<void*>(uiToken))!=D2RL::Threads::Result::Success) {
        std::lock_guard lock(mutex);uiCallback.Claim(uiToken);
    }
    if(gameToken&&threads->runOnGameThread(context,&Apply,reinterpret_cast<void*>(gameToken))!=D2RL::Threads::Result::Success) {
        std::lock_guard lock(mutex);gameCallback.Claim(gameToken);
    }
}
void Submit(const Shot& shot) {
    uintptr_t token=0;
    {
        std::lock_guard lock(mutex);
        if(!ready.load()||!enabled.load()||!once.Take(shot,current,GetTickCount64())){++stale;return;}
        if(pendingCount==pending.size()){++refused;return;}
        pending[pendingCount++]=shot;++submitted;
        token=gameCallback.Start(GetTickCount64());
    }
    if(token&&threads->runOnGameThread(context,&Apply,reinterpret_cast<void*>(token))!=D2RL::Threads::Result::Success) {
        std::lock_guard lock(mutex);if(gameCallback.Claim(token))pendingCount=0;++refused;
    }
}
void Created(uintptr_t game,uintptr_t params,uintptr_t unit) {
    if(launch.slot<0||launch.created||launch.game!=game)return;
    uintptr_t owner=0,path=0;uint32_t h[3]{};int velocity=0;
    if(!Read(params+8,owner)||owner!=launch.player||!Read(unit,h)||h[0]!=3||h[1]!=uint32_t(launch.shot.kind)||
       !Read(unit+0x38,path)||!Read(path+0xa0,velocity))return;
    Arrow a;a.game=game;a.server=unit;a.serverId=h[2];a.flight.shot=launch.shot;
    a.flight.speed=Speed(velocity,launch.shot.draw);a.flight.alive=true;a.flight.updatedAt=GetTickCount64();
    a.room=reinterpret_cast<uintptr_t(*)(uintptr_t)>(d2rsig::Addr(RoomRva))(unit);
    {std::lock_guard lock(mutex);arrows[launch.slot]=a;Recount();}
    launch.created=true;++spawned;
    lastLatency=unsigned(GetTickCount64()-launch.shot.at);lastSpeedCm=unsigned(a.flight.speed*100);
}
bool Visual(uint32_t entity,uintptr_t unit,bool hasReleasePoint) {
    uint32_t header[3]{};
    if(!Read(unit,header)||header[0]!=3||!BasicMissile(header[1]))return false;
    std::lock_guard lock(mutex);
    if(header[1]==0&&preview.pose.session==current.session&&((preview.active&&previewCreatingSession&&previewCreatingSession==current.session)||
       (preview.client==unit&&preview.clientId==header[2]))) {
        if(!preview.binding.Bind(entity,hasReleasePoint))return false;
        preview.client=unit;preview.clientId=header[2];Recount();return true;
    }
    for(size_t i=0;i<arrows.size();++i) {
        auto& a=arrows[i];
        if(header[1]==uint32_t(a.flight.shot.kind)&&a.flight.shot.sequence&&a.flight.shot.session==current.session&&((a.client==unit&&a.clientId==header[2])||
            (creatingVisual==int(i)&&a.flight.shot.session==visualSession&&a.flight.shot.sequence==visualSequence))) {
            const bool wasBound=a.visualBinding.Bound();
            if(!a.visualBinding.Bind(entity,hasReleasePoint)){++ignoredEntities;return false;}
            if(!wasBound)++boundMeshes;
            a.client=unit;a.clientId=header[2];Recount();return true;
        }
    }
    return false;
}
void Install() {
    if(ready.load()||!enabled.load()||!context||!threads||!threads->runOnGameThread||!threads->runOnUiThread||!projectiles::Ready())return;
    if(!checked) {
#define CHECK(name) if(!d2rsig::Check(name##Rva,name##Sig,sizeof name##Sig))return
        CHECK(Attack);CHECK(Throw);CHECK(ThrowCall);CHECK(Shoot);CHECK(Collision);CHECK(Hit);CHECK(ClientTick);CHECK(ClientKill);CHECK(Transform);
        CHECK(MissileSound);CHECK(TravelSoundCall);
        CHECK(AttackCall);CHECK(ShootParams);CHECK(VelocityLayout);CHECK(WorldLayout);
        CHECK(FrameGet);CHECK(FrameSet);CHECK(Room);CHECK(RoomPoint);CHECK(Mask);CHECK(Stats);CHECK(Relation);CHECK(IsBow);CHECK(MissileType);
#undef CHECK
        checked=true;
    }
    if(!shootIn)shootIn=d2rsig::Hook(ShootRva,ShootSig,sizeof ShootSig,reinterpret_cast<void*>(&Shoot),reinterpret_cast<void**>(&originalShoot));
    if(!collisionIn)collisionIn=d2rsig::Hook(CollisionRva,CollisionSig,sizeof CollisionSig,reinterpret_cast<void*>(&Collision),reinterpret_cast<void**>(&originalCollision));
    if(!clientIn)clientIn=d2rsig::Hook(ClientTickRva,ClientTickSig,sizeof ClientTickSig,reinterpret_cast<void*>(&ClientTick),reinterpret_cast<void**>(&originalClientTick));
    if(!killIn)killIn=d2rsig::Hook(ClientKillRva,ClientKillSig,sizeof ClientKillSig,reinterpret_cast<void*>(&ClientKill),reinterpret_cast<void**>(&originalClientKill));
    if(!transformIn)transformIn=d2rsig::Hook(TransformRva,TransformSig,sizeof TransformSig,reinterpret_cast<void*>(&Transform),reinterpret_cast<void**>(&originalTransform));
    if(!soundIn)soundIn=d2rsig::Hook(MissileSoundRva,MissileSoundSig,sizeof MissileSoundSig,reinterpret_cast<void*>(&MissileSound),reinterpret_cast<void**>(&originalMissileSound));
    if(shootIn&&collisionIn&&clientIn&&killIn&&transformIn&&soundIn&&!ready.exchange(true))
        framevr::logging::Write("Frame physical projectiles: tracked native-arrow preview and frozen 6DoF arrow/javelin flight; terrain-correct grid/room queries; mesh-only transforms; native basic attack/throw costs");
}
void Report() {
    const auto s=submitted.exchange(0),n=spawned.exchange(0),v=visuals.exchange(0),x=stale.exchange(0),r=refused.exchange(0);
    const auto c=contacts.exchange(0),w=walls.exchange(0),e=expired.exchange(0);
    const auto b=boundMeshes.exchange(0),i=ignoredEntities.exchange(0);
    const auto p=positionMismatch.exchange(0);
    const auto pc=previewCreated.exchange(0),pr=previewRefused.exchange(0),ur=uiRecovered.exchange(0),gr=gameRecovered.exchange(0);
    if(!s&&!n&&!v&&!x&&!r&&!c&&!w&&!e&&!b&&!i&&!pc&&!pr&&!ur&&!gr)return;
    char line[480];snprintf(line,sizeof line,"Frame physical projectiles: releases=%u native=%u visual=%u stale=%u refused=%u contacts=%u barriers=%u expired=%u meshes=%u ignored entities=%u position_mismatch=%u preview_created=%u preview_refused=%u ui_recovered=%u game_recovered=%u last latency=%ums speed=%.1fm/s",s,n,v,x,r,c,w,e,b,i,p,pc,pr,ur,gr,lastLatency.load(),lastSpeedCm.load()/100.f);
    framevr::logging::Write(line);
}
}
