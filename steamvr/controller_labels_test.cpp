#include "controller_labels.h"
#include "interaction_paint.h"
#include <cassert>
#include <cstdio>
using namespace framevr;
int main(){
    using namespace controllers;
    assert(Detect("/interaction_profiles/oculus/touch_controller")==Layout::Touch);
    assert(Detect("/interaction_profiles/meta/touch_controller_plus")==Layout::Touch);
    assert(Detect("/interaction_profiles/facebook/touch_controller_pro")==Layout::Touch);
    assert(Detect("/interaction_profiles/valve/index_controller")==Layout::Index);
    assert(Detect(FrameProfile)==Layout::Frame);
    assert(Detect("")==Layout::Generic&&Detect("/unknown/touch_controller")==Layout::Generic);
    Labels detected{{Layout::Touch,Layout::Index}};
    const auto automatic=Resolve(Choice::Automatic,detected);
    assert(automatic==detected); // Mixed controllers, not an HMD-brand guess.
    assert(automatic.Slot(5)=="Left A"&&automatic.Slot(9)=="Left Grip + B");
    const auto quest=Resolve(Choice::Quest,detected);
    assert(quest.Slot(5)=="Left X"&&quest.Slot(6)=="Left Y");
    assert(quest.Slot(7)=="Left Grip + X"&&quest.Slot(9)=="Left Grip + Y");
    assert(quest.Slot(8)=="Right A"&&quest.Slot(1)=="Right B");
    assert(quest.Slot(2)=="Right Grip + A"&&quest.Slot(3)=="Right Grip + B");
    assert(quest.Slot(0)=="Right Trigger"&&quest.Slot(4)=="Left Trigger");
    const auto frame=Resolve(Choice::Frame,detected);
    assert(frame.Slot(5)=="Left D-pad Down"&&frame.Slot(9)=="Left Grip + D-pad Up");
    assert(std::string(FacePath(Layout::Frame,1,false))=="dpad_down/click");
    assert(std::string(FacePath(Layout::Frame,1,true))=="dpad_up/click");
    assert(std::string(FacePath(Layout::Touch,1,false))=="x/click");
    assert(std::string(FacePath(Layout::Index,1,true))=="b/click");
    assert(Labels{}.Slot(5)=="Left Primary"&&Labels{}.Slot(6)=="Left Secondary");
    assert(Labels{}.Slot(-1).empty()&&Labels{}.Slot(skills::Count).empty());
    assert(Resolve(Choice::Generic,detected)==Labels{});
    const Labels wmr{{Layout::Motion,Layout::Motion}};
    assert(wmr.Slot(9)=="Left Unbound");
    // A pending atlas must be rejected after either detection or override changes.
    interaction::Snapshot s;interaction::Output o;
    assert(interaction::Context(s,o,true,quest)!=interaction::Context(s,o,true,frame));
    auto changed=detected;changed.hands[0]=Layout::Generic;
    assert(interaction::Context(s,o,true,changed)!=interaction::Context(s,o,true,detected));
    puts("Controller detection, per-hand labels, overrides, bindings and atlas invalidation passed");
}
