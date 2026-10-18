#include "ui_world.h"
#include "native_lock.h"
#include "generation_cache.h"
#include "hook_metrics.h"
#include <windows.h>
#include <d3dcompiler.h>
#include <reshade.hpp>
#include <atomic>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace framevr::uiworld {
namespace {
using namespace reshade::api;
NativeMutex mutex;
std::atomic<uint64_t> generation{1};
std::unordered_map<uint64_t,pipeline> copies;
std::atomic<bool> active{false},ready{false},used{false};
thread_local bool ours=false;
void (*logger)(const char*)=nullptr;
std::mutex compileMutex;
bool compiled=false;
std::vector<uint8_t> code;
uint64_t Hash(const void* bytes,size_t size) {
    uint64_t h=1469598103934665603ull;
    for(size_t i=0;i<size;++i)h=(h^static_cast<const uint8_t*>(bytes)[i])*1099511628211ull;
    return h;
}
void Compile() {
    // Verified SDR final composite (5d76441e00c5b627): t10/s0 is the finished
    // world, t11/s1 is the native UI. Sampling t10 alone is exactly its result
    // with empty UI (RGB=0, transmittance=1). Keep its TEXCOORD1 linkage.
    constexpr char source[]=R"(
Texture2D<float4> world : register(t10);
SamplerState worldSampler : register(s0);
struct NativeVertex {
    float4 position:SV_Position;
    float2 uv0:TEXCOORD0;
    float2 uv1:TEXCOORD1;
};
float4 PS(NativeVertex input):SV_Target {return float4(world.Sample(worldSampler,input.uv1).rgb,1);}
)";
    const auto library=LoadLibraryW(L"d3dcompiler_47.dll");
    const auto compile=library?reinterpret_cast<decltype(&D3DCompile)>(GetProcAddress(library,"D3DCompile")):nullptr;
    ID3DBlob *shader=nullptr,*errors=nullptr;
    if(compile&&SUCCEEDED(compile(source,sizeof source-1,"vr-ui-world",nullptr,nullptr,"PS","ps_5_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&shader,&errors))) {
        const auto* begin=static_cast<const uint8_t*>(shader->GetBufferPointer());code.assign(begin,begin+shader->GetBufferSize());
    }
    if(shader)shader->Release();if(errors)errors->Release();if(library)FreeLibrary(library);
    if(code.empty()&&logger)logger("Frame panels: world compositor unavailable; retaining flat menu fallback");
}
void Created(device* device,pipeline_layout layout,uint32_t count,const pipeline_subobject* parts,pipeline original) {
    if(ours)return;
    for(uint32_t i=0;i<count;++i) {
        if(parts[i].type!=pipeline_subobject_type::pixel_shader||!parts[i].data)continue;
        const auto& native=*static_cast<const shader_desc*>(parts[i].data);
        if(!native.code||native.code_size!=1356||Hash(native.code,native.code_size)!=0x5d76441e00c5b627ull)return;
        {std::lock_guard lock(compileMutex);if(!compiled){Compile();compiled=true;}}
        if(code.empty())return;
        auto shader=native;shader.code=code.data();shader.code_size=code.size();
        std::vector<pipeline_subobject> description(parts,parts+count);description[i].data=&shader;description[i].count=1;
        pipeline replacement{};ours=true;
        const bool ok=device->create_pipeline(layout,count,description.data(),&replacement);ours=false;
        if(!ok){if(logger)logger("Frame panels: verified compositor pipeline creation failed; retaining flat menu fallback");return;}
        {std::lock_guard lock(mutex);copies.emplace(original.handle,replacement);++generation;}
        if(!ready.exchange(true)&&logger)logger("Frame panels: verified SDR world-only composite ready for both eyes");
        return;
    }
}
void Destroyed(device* device,pipeline original) {
    if(ours)return;
    pipeline replacement{};
    {std::lock_guard lock(mutex);auto it=copies.find(original.handle);if(it==copies.end())return;
        replacement=it->second;copies.erase(it);ready.store(!copies.empty());++generation;}
    ours=true;device->destroy_pipeline(replacement);ours=false;
}
void Bind(command_list* commands,pipeline_stage stages,pipeline original) {
    hookmetrics::Scope measured(hookmetrics::UiWorldBind);
    if(ours||!active.load(std::memory_order_relaxed)||uint32_t(stages&pipeline_stage::all_graphics)==0)return;
    pipeline replacement{};
    static thread_local GenerationCache<pipeline> cache;
    const auto version=generation.load(std::memory_order_acquire);
    if(!cache.Get(original.handle,version,replacement)) {
        std::lock_guard lock(mutex);auto it=copies.find(original.handle);
        if(it!=copies.end())replacement=it->second;
        cache.Put(original.handle,version,replacement);
    }
    if(!replacement.handle)return;
    ours=true;commands->bind_pipeline(stages,replacement);ours=false;used.store(true);
}
}
void Register(void (*log)(const char*)) {
    logger=log;
    reshade::register_event<reshade::addon_event::init_pipeline>(&Created);
    reshade::register_event<reshade::addon_event::destroy_pipeline>(&Destroyed);
    reshade::register_event<reshade::addon_event::bind_pipeline>(&Bind);
}
bool Ready(){return ready.load();}
bool Used(){return used.load();}
void SetActive(bool on){active.store(on);if(!on)used.store(false);}
}
