#pragma once
#include <cstdint>
#include "melee_contact.h"
#include "bow_flight.h"

// Called by the existing game-side stereo draw hook and ReShade integration.
// The OpenXR session uses the game's D3D12 device/queue.
namespace framevr {
using LogFn = void (*)(const char*);
using PanelFn = bool (*)();
void Register(LogFn log, PanelFn wantsPanel, PanelFn inWorld, PanelFn toggleMenus);
// Called by the owning window's WndProc, without the transport/render lock.
bool HandleWindowFocusMessage(void* window,uint32_t message,uint64_t token,int64_t deadline);
void ConfigureDeferredWait(bool enabled);
void ConfigureSymmetricProjection(bool enabled);
void ConfigureEyeTracking(bool enabled);
void ConfigureGameplay(bool enabled);
void ConfigureInteractions(bool seated,float waistBelowEyes,float beltForward,int dominantHand);
// Current motion-combat aim in the existing head-yaw-relative hand frame.
bool SettingsOpen();
void SetFrontendScene(bool active); // render-only UI separation, never gameplay
bool OwnsControllerRumble(); // lock-free; VR input owns feedback instead of gamepad motors
bool SpellSelected();
bool LeftHandAiming();
bool CrossbowAiming();
bool NativeAimOwnership(); // hand casting or wheel: preserve locomotion, suppress legacy aim modes
bool NativeSkillCasting(); // any active trigger/quick cast; retain native locomotion input
bool DirectedSpellActive(); // fresh native assignment, actually casting (wheel or quick shortcut)
bool GameplayRay(float origin[3],float direction[3]);
bool MeleeSample(uint32_t poseStamp,melee::Sample& sample);
bool ShieldSample(uint32_t poseStamp,melee::Sample& sample);
bool RequestShieldSkill(const melee::Sample& sample,uint32_t target,math::Vec gridTarget);
int TakeShieldTarget(uint32_t& target); // 1 physical target, -1 cancelled cast, 0 native fallback
bool ShieldAim(float& x,float& y);
void FinishShieldSkill(bool accepted);
void MeleeImpact(); // thread-safe; haptic delivered by XR input update
void SpellCast(int hand,uint64_t session); // one successful hand spell, not each projectile
bool TakeBowShot(bowflight::Shot& shot);
bool TakeCrossbowShot(bowflight::Shot& shot);
bool CrossbowHeld(const bowflight::Shot& shot);
bool TakeSpellShot(int hand,bowflight::Shot& shot);
bool SpellHeld(const bowflight::Shot& shot);
bool BowPreviewPose(bowflight::Shot& pose,bool& visible);
bowflight::Gate BowGate();
void SetBowWorldEpoch(uint32_t epoch);
uint64_t BowRenderTime(); // one timestamp for both eyes, lock-free
struct FrameTiming {
    double waitFrameMs=0,waitGpuMs=0,submitMs=0,maxPipelineMs=0,locateMs=0,inputMs=0,releaseMs=0;
    double displayPeriodMs=0; // Runtime-predicted application period; may differ from panel refresh.
    double acquireMs[2]{},copySubmitMs[2]{},gpuSceneMs[2]{},gpuEffectsMs[2]{},gpuCopyMs[2]{};
    uint32_t pipelines=0;bool gpuValid=false;
};
FrameTiming LastTiming();
#if D2RVR_EXPERIMENTAL_PERSPECTIVES
// Called only at a game stereo-pair boundary. Does not recreate the XR session.
void SetExternalPerspective(int mode);
void ObserveMouseCursor(void* cursor); // live native cursor, no replacement artwork
bool PerspectiveReady();
bool DioramaSkyboxRays(float rays[18]); // tracking-space directions, no table transform or translation
void ConfigureDioramaPassthrough(bool requested);
void DioramaShaderReady(bool ready);
bool DioramaPassthroughActive();
void ConfigureDioramaPlacement(float height,float floorOffset,float width);
void ObserveHudPad(unsigned slot,bool connected,bool active,uint8_t leftTrigger);
void ConfigureHeadHud(float vitalsY,float xpY,float beltY,float vitalsX,float vitalsScale,float xpScale,float beltScale,float beltX,float skillsY,float skillsScale);
void ConfigureSpatialUi(float hudTilt,float hudArc,float panelWidth,float panelHeight);
void ObserveDialogueTarget(float x,float z);
uint32_t DioramaPad(unsigned slot,bool connected,uint16_t buttons,bool allowed,uint32_t epoch,uint8_t lt,uint8_t rt);
bool TakeDioramaHeight(float& height,float& width,bool allowed);
void RecenterTable();
bool DioramaView(float out[16],float center[16],float sampled[3],const float hero[3],
    float width,float radius,float distance,float drop,float follow,float heading,int eye);
bool AlternativeView(float out[16],float center[16],const float hero[3],float height,
    float distance,float heading,float down,float units,uint32_t generation,int eye);
#endif
void Shutdown(bool preserveResolution=false);
void BeginPair();
void BeginEye(int eye);
// Diagnostic only: F8 saves the next complete gameplay pair via ReShade.
void RequestStereoCapture();
void EndPair();
void ResetHeight();
void Recenter();
bool TakeWorldRecenter();
bool Active();
// Any thread: a focused VR session with motion controls ran input recently.
// False in desktop/flat play, so VR-only world effects stay out of it.
bool VrGameplay(uint64_t now);
bool SpatialMenu();
// mapNorth is the unmodified native camera's horizontal heading (map-up).
bool View(float out[16], const float hero[3], float eyeHeight,
          float userEyeHeight, float bodyHeading, float recenterYaw, int eye, float mapNorth);
bool Projection(float out[16], float nearWorld, int eye);
}
