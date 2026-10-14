#include "interactions.h"
#include "interaction_icons.h"
#include "interaction_paint.h"
#include "potion_scene.h"
#include <cstdio>
#include <cstdlib>
#include <limits>
using namespace framevr::interaction;
namespace math=framevr::math;
static void Check(bool b,const char* s){if(!b){std::fprintf(stderr,"FAIL: %s\n",s);std::exit(1);}}
struct Sim {
    Engine e;Input in;Output out;
    Sim(){in.active=true;in.tracked[0]=in.tracked[1]=true;in.session=10;in.epoch=1;in.head.p={0,1.7f,0};
        in.hand[0].p={.25f,1.3f,-.4f};in.hand[1].p={-.25f,1.3f,-.4f};
        in.snapshot.session=10;in.snapshot.equipmentKnown=in.snapshot.beltKnown=true;in.snapshot.activeSet=1;
        auto& b=in.snapshot.belt[0];b.known=true;b.count=2;b.revision=4;b.items[0]={42,1,Code('h','p','1')};b.items[1]={43,2,Code('m','p','1')};
        Step(3);}
    Output Step(int count=1){while(count--){in.time+=.02;in.at=1000000+uint64_t(in.time*1000);in.snapshot.at=in.at;out=e.Step(in);}return out;}
};
int main(){
    for(int dominant=0;dominant<2;++dominant)for(float side:{-.23f,.23f}){
        Sim s;Config c;c.dominant=dominant;s.e.Configure(c);s.Step(4);
        s.in.hand[dominant].p=s.in.head.p+Vec{side,-.2f,.14f};s.Step(10);
        s.in.trigger=1;
        Check(s.Step().request.action==Action::None&&!s.out.block,"trigger behind shoulder remains available for javelin wind-up");
        s.in.trigger=0;s.Step();s.in.grip[dominant]=1;
        const auto r=s.Step().request;
        Check(r.action==Action::Equip&&r.index==2&&s.out.block,"dominant grip at either shoulder requests opposite native set and blocks attacks");
        for(int i=0;i<12;++i)Check(s.Step().request.action==Action::None,"one shoulder press issues exactly one request");
        s.in.reply={r.sequence,s.in.session,Result::Applied};s.in.snapshot.activeSet=2;
        Check(s.Step().result==Result::Applied&&s.out.feedbackHand==dominant,"shoulder swap waits for native equipment confirmation and pulses the dominant hand");
        s.in.grip[dominant]=0;s.Step(3);
        s.in.grip[dominant]=1;
        Check(s.Step().request.index==1,"second deliberate shoulder press returns to set I");
    }
    {Sim s;s.in.hand[0]=s.out.holster[1];s.in.grip[0]=1;s.Step();s.in.hand[0].p.z-=.2f;
     Check(s.Step().request.action==Action::None,"hip pulls are disabled in the default controls");}
    {
        Sim s;
        // Resting hands previously froze the waist, even for a small step or turn.
        s.in.hand[0].p=s.out.belt[0].p;s.in.hand[1].p=s.out.belt[3].p;
        s.in.head.p={.04f,1.66f,.02f};s.in.head.q=math::Yaw(.25f);s.Step();
        auto center=(s.out.belt[0].p+s.out.belt[3].p)*.5f;
        auto delta=center-s.in.head.p;
        Check(std::fabs(delta.x+.26f*std::sin(.25f))<1e-5f&&
            std::fabs(delta.z+.26f*std::cos(.25f))<1e-5f&&std::fabs(delta.y+.57f)<1e-5f,
            "lowered hands cannot freeze front belt during a small turn, step or crouch");
        const math::Quat pitch{std::sin(-Pi/6),0,0,std::cos(-Pi/6)};
        const math::Quat roll{0,0,std::sin(.3f),std::cos(.3f)};
        s.in.head.p={1.2f,1.55f,.7f};
        s.in.head.q=math::Multiply(math::Yaw(Pi/2),math::Multiply(pitch,roll));s.Step();
        for(int i=0;i<4;++i) {
            const auto p=s.out.belt[i].p,up=math::Rotate(s.out.belt[i].q,{0,1,0});
            Check(std::fabs(p.x-.94f)<1e-5f&&std::fabs(p.y-.98f)<1e-5f&&
                std::fabs(p.z-(.7f-(i-1.5f)*.095f))<1e-5f&&up.y>.99999f,
                "quarter turn follows immediately; downward gaze and roll keep bottles upright at waist");
        }
        // Avoid an arbitrary 180-degree flip around a straight-down gaze.
        for(float tilt:{-Pi/2-.015f,-Pi/2,-Pi/2+.015f}) {
            s.in.head.q=math::Multiply(math::Yaw(-.8f),{std::sin(tilt/2),0,0,std::cos(tilt/2)});s.Step();
            Check(std::fabs(math::Heading(s.out.body.q)-Pi/2)<1e-5f,
                "near-vertical gaze retains last reliable waist heading");
        }
        s.in.head.q=math::Yaw(-Pi/2);s.Step();
        Check(std::fabs(s.out.belt[0].p.x-1.46f)<1e-5f,"belt resumes following when horizontal heading returns");
        s.in.snapshot.belt[1]=s.in.snapshot.belt[0];s.in.snapshot.belt[1].items[0].handle=99;
        s.in.hand[0]=s.out.belt[0];s.in.grip[0]=1;s.Step();
        Check(s.out.bottle==0,"grab uses the moved belt position");
        s.in.head.p={2,1.4f,1};s.in.head.q=math::Yaw(0);s.in.hand[0].p={2.3f,1.2f,.8f};s.Step();
        auto scene=framevr::potions::MakeScene(s.in,s.out);
        Check(s.out.bottle==0&&s.out.bottleItem.handle==42&&scene.count==2&&
            std::fabs(scene.items[0].pose.p.z-.74f)<1e-5f&&
            std::fabs(scene.items[1].pose.p.x-2.3f)<1e-5f&&s.out.request.action==Action::None,
            "belt follows while drinking hand moves independently; grabbed identity and other bottle remain intact");
        s.in.grip[0]=0;s.Step();scene=framevr::potions::MakeScene(s.in,s.out);
        Check(s.out.bottle<0&&scene.count==2&&std::fabs(scene.items[0].pose.p.z-.74f)<1e-5f,
            "unused bottle returns to current waist position, not original grab location");
        Config seated;seated.seated=true;s.e.Configure(seated);s.Step(3);
        Check(std::fabs(s.out.belt[0].p.y-1.05f)<1e-5f,"body-following belt retains seated reach height");
    }
    {
        Sim s;Config legacy;legacy.holsters=true;s.e.Configure(legacy);s.Step(3);s.in.hand[0]=s.out.holster[1];s.in.grip[0]=1;s.Step();
        const auto yaw=math::Yaw(.8f);const Vec step{.3f,0,.2f};
        for(auto* p:{&s.in.head,&s.in.hand[0],&s.in.hand[1]}) {
            p->p=math::Rotate(yaw,p->p)+step;p->q=math::Multiply(yaw,p->q);
        }
        Check(s.Step().owner[0]==Owner::Holster&&s.out.request.action==Action::None,
            "turning and stepping with hand on attached holster cannot swap weapons");
        s.in.hand[0].p=s.in.hand[0].p+math::Rotate(yaw,{0,0,-.15f});
        Check(s.Step().request.action==Action::Equip&&s.out.request.index==2,
            "intentional pull relative to moving body still draws the chosen weapon set");
    }
    for(int slot=4;slot<8;++slot) {
        Sim s;s.in.snapshot.bindingsEditable=true;s.in.snapshot.bindingBank=0;s.in.snapshot.skillCount=1;
        for(auto& skill:s.in.snapshot.skills)skill.known=true;
        auto& skill=s.in.snapshot.catalog[0];skill.id=70;skill.available=skill.known=true;
        auto click=[&](int hit){s.in.trigger=0;s.Step();s.in.hover=hit;s.in.trigger=1;return s.Step();};
        s.in.wheel=true;click(11);Check(click(slot).editSlot==slot,"alternate wheel slot opens editor");
        const auto r=click(0).request;
        Check(r.action==Action::BindSkill&&r.index==slot&&r.skill==70&&CanBind(s.in.snapshot,r,s.in.at),"all LT variant slots use validated native assignment");
    }
    {
        Sim s;s.in.hand[1].p=s.out.belt[0].p;s.in.quickCast=true;s.in.grip[1]=1;
        Check(s.Step().bottle<0&&s.out.owner[1]!=Owner::Bottle,"alternate shortcut grip cannot grab belt potion");
    }
    {
        Sim s;s.in.snapshot.bindingsEditable=true;s.in.snapshot.bindingBank=0;s.in.snapshot.skillCount=10;
        for(int i=0;i<8;++i)s.in.snapshot.skills[i].known=true;
        for(int i=0;i<10;++i){auto& skill=s.in.snapshot.catalog[i];skill.id=36+i;skill.available=skill.known=true;}
        auto click=[&](int hit){s.in.trigger=0;s.Step();s.in.hover=hit;s.in.trigger=1;return s.Step();};
        s.in.wheel=true;
        Check(click(11).editor==1&&s.out.select==-2,"editor does not select a combat slot");
        Check(click(2).editor==2&&s.out.editSlot==2&&s.out.editCount==8,"unassigned slot opens available skills");
        Check(click(10).editPage==1&&s.out.editCount==2,"skill pagination bounded by actual skills");
        Check(WheelCell(.1f,.25f,-1,MenuPage::EditSkills)==0&&WheelCell(.8f,.15f,-1,MenuPage::EditSkills)==8,"skill rows and clear button hit regions");
        Check(WheelCell(.5f,.25f,-1,MenuPage::EditSkills)==-1,"skill column gap cannot assign");
        // Choices stay fixed while this page is displayed. Native validation
        // checks the chosen identity, never a list index that can be reordered.
        s.in.snapshot.catalog[0].id=200;
        const auto request=click(0).request;
        Check(request.action==Action::BindSkill&&request.index==2&&request.skill==44&&request.bank==0&&s.out.select==-2,"binding names exact skill and slot without casting");
        Check(CanBind(s.in.snapshot,request,s.in.at),"fresh known learned skill can bind");
        for(int i=0;i<10;++i)Check(s.Step().request.action==Action::None&&s.out.block,"held trigger cannot repeat binding or cast");
        auto changed=s.in.snapshot;changed.bindingBank=16;
        Check(!CanBind(changed,request,s.in.at),"weapon-set change refuses stale binding");
        changed=s.in.snapshot;changed.skills[2].id=70;
        Check(!CanBind(changed,request,s.in.at),"concurrent slot change is not overwritten");
        changed=s.in.snapshot;changed.catalog[8].owner=123;
        Check(!CanBind(changed,request,s.in.at),"different item owner is not the chosen skill");
        Check(!CanBind(s.in.snapshot,request,s.in.at+651),"stale catalog refuses write");
        s.in.reply={request.sequence,10,Result::Applied};s.in.snapshot.skills[2]=s.in.snapshot.catalog[8];
        Check(s.Step().editor==1&&s.out.result==Result::Applied,"native readback returns to slots");
        click(2);const auto clear=click(8).request;
        Check(clear.action==Action::BindSkill&&clear.skill==-1&&clear.previousSkill==44&&CanBind(s.in.snapshot,clear,s.in.at),"clear is an explicit validated native request");
        s.in.cancel=true;Check(s.Step().cancelPending&&!s.out.wheel&&s.out.editor==0,"cancel drops pending edit and requires rearm");
        s.in.cancel=false;s.in.wheel=false;s.in.trigger=0;s.Step();s.in.wheel=true;click(11);click(1);
        s.in.tracked[0]=false;
        Check(s.Step().editor==0&&s.out.cancelPending&&s.out.block,"tracking loss closes editor without binding");
    }
    Check(icons::ItemKey({1,1,Code('h','p','1')})=="i20317068","native little-endian item icon code");
    Check(icons::ItemKey({1,1,Code('h','p','1',0)})=="i20317068","NUL-padded native item code");
    Check(icons::ItemKey({1,1,Code('h','a','x'),7,0})=="u0","unique item uses its record artwork");
    Check(icons::ItemKey({1,1,Code('h','a','x'),5,4})=="t4","set item uses its record artwork");
    Check(icons::ItemKey({}).empty(),"empty slot has no stale icon");
    {
        Sim s;s.in.hand[1].p={-.18f,1.35f,-.24f};
        Check(!s.Step().wrist,"natural grip orientation hides wrist vitals");
        s.in.hand[1].q={0,0,1,0};
        Check(!s.Step().wrist,"old upside-down +/-Y assumption is not a watch gesture");
        s.in.hand[1].q={0,0,.70710678f,.70710678f};
        Check(!s.Step().wrist,"left palm facing eyes does not reveal back-of-wrist watch");
        s.in.hand[1].q={0,0,-.70710678f,.70710678f};
        Check(s.Step().wrist,"OpenXR left grip -X rolled upward reveals watch");
        const auto panel=WristPanel(s.in.hand[1],s.in.head);
        const auto normal=math::Rotate(panel.q,{0,0,1});
        Check(math::Dot(normal,math::Normalized(s.in.head.p-panel.p))>.65f,
            "revealed panel front faces the eyes, not away from them");
        Check(math::Dot(panel.p-s.in.hand[1].p,normal)>.05f,
            "panel offset is on the same wrist face as its front");
        const auto headUp=math::Rotate(s.in.head.q,{0,1,0});
        Check(math::Dot(math::Rotate(panel.q,{0,1,0}),headUp)>=-1e-5f,
            "watch text cannot be upside down relative to the head");
        s.in.tracked[0]=false;
        Check(s.Step().wrist&&s.out.block&&s.out.request.action==Action::None,
            "untracked resting right hand keeps passive watch, blocks combat");
        s.in.tracked[0]=true;s.Step(2);
        s.in.tracked[1]=false;Check(!s.Step().wrist,"untracked left wrist hides watch");
        s.in.tracked[1]=true;s.Step(2);
        s.in.active=false;Check(!s.Step().wrist,"native menu hides passive watch");
        s.in.active=true;s.Step(2);
        const auto yaw=math::Yaw(1.2f);
        for(auto* p:{&s.in.hand[1],&s.in.head}){p->p=math::Rotate(yaw,p->p);p->q=math::Multiply(yaw,p->q);}
        Check(s.Step().wrist,"wrist reveal is independent of world heading");
        s.in.hand[1].p=s.in.head.p+Vec{0,-.8f,0};
        Check(!s.Step().wrist,"distant hand does not reveal panel");
        s.in.hand[1].p=s.in.head.p;
        Check(!s.Step().wrist,"coincident hand and eyes cannot reveal panel");
        s.in.hand[1].p.x=std::numeric_limits<float>::quiet_NaN();
        Check(!s.Step().wrist,"lost wrist tracking hides panel");
    }
    {
        // Sweep rotations including head-up parallel to the watch normal.
        // Every rendered face must stay on -X with a unit, finite quaternion.
        Pose hand{{-.18f,1.35f,-.24f}},head{{0,1.7f,0}};
        for(int roll=-180;roll<=180;roll+=5)for(int pitch=-90;pitch<=90;pitch+=5) {
            hand.q={0,0,std::sin(roll*Pi/360),std::cos(roll*Pi/360)};
            head.q={std::sin(pitch*Pi/360),0,0,std::cos(pitch*Pi/360)};
            const auto p=WristPanel(hand,head);const auto q=p.q;
            Check(Valid(p)&&std::fabs(q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w-1)<1e-5f,
                "watch panel stays valid through head/wrist rotations");
            Check(math::Dot(math::Rotate(q,{0,0,1}),WristNormal(hand))>.99999f,
                "readability adjustment preserves back-of-wrist face");
        }
    }
    for(int i=0;i<8;++i){float a=i*Pi/4+WheelPhase;Check(WheelCell(.5f+std::sin(a)*.28f,.45f-std::cos(a)*.28f)==i,"wedge centers agree");}
    Check(WheelCell(.5f,.45f)==8&&WheelCell(.18f,.93f)==9,"weapon center and utility tab");
    Check(WheelCell(.38f,.93f)==12&&WheelCell(.62f,.93f)==10&&WheelCell(.86f,.93f)==11,"map, settings and skill editor tabs");
    Check(WheelCell(.5f,.57f)==-1,"dead zone cannot select");
    Check(WheelCell(.26f,.94f)==-1&&WheelCell(.5f,.99f)==-1,"footer gap and outside frame cannot select");
    for(auto page:{MenuPage::EditSlots,MenuPage::EditSkills})
        Check(WheelCell(.18f,.93f,-1,page)==9&&WheelCell(.5f,.93f,-1,page)==10&&WheelCell(.82f,.93f,-1,page)==11,
            "editor keeps three original footer controls");
    for(int i=0;i<8;++i) {
        const float a=i*Pi/4+WheelPhase;
        Check(WheelCell(.5f+std::sin(a)*.28f,.45f-std::cos(a)*.28f,i,MenuPage::Map)==-1,
            "map cannot hit hidden spell wedges even with previous hover");
    }
    Check(WheelCell(.5f,.45f,-1,MenuPage::Map)==-1&&WheelCell(.38f,.93f,-1,MenuPage::Map)==12,
        "map body is passive while footer remains interactive");
    {
        Sim s;auto click=[&](int hit){s.in.trigger=0;s.Step();s.in.hover=hit;s.in.trigger=1;return s.Step();};
        s.in.hand[1].p={-.18f,1.35f,-.24f};s.in.hand[1].q={0,0,-.70710678f,.70710678f};
        s.Step();Check(s.out.wrist&&!MapWanted(s.in,s.out,s.in.at),"watch vitals no longer request minimap");
        s.in.wheel=true;click(12);
        Check(s.out.wheel&&s.out.map&&!s.out.utility&&s.out.block&&s.out.select==-2&&s.out.request.action==Action::None&&
            MapWanted(s.in,s.out,s.in.at)&&s.e.Page()==MenuPage::Map,"map tab opens without changing selected spell or requesting a native action");
        const auto mapContext=Context(s.in.snapshot,s.out,true);auto spells=s.out;spells.map=false;
        Check(mapContext!=Context(s.in.snapshot,spells,true),"map page invalidates a previously painted spell page");
        Check(!MapWanted(s.in,s.out,s.in.at+651),"stale character snapshot cannot display map");
        for(int i=0;i<8;++i)Check(s.Step().map&&s.out.select==-2&&s.out.request.action==Action::None,"held map selection cannot cast");
        for(int cell:{0,4,8})Check(click(cell).map&&s.out.wheel&&s.out.select==-2&&s.out.request.action==Action::None,
            "even stale body hits cannot select a spell, potion or basic attack on map");
        Check(!click(9).map&&!s.out.utility&&s.out.wheel&&s.out.select==-2&&s.e.Page()==MenuPage::Spells,
            "SPELLS returns from map without toggling equipment or selecting a spell");
        click(9);Check(s.out.utility,"equipment page still opens");click(12);Check(s.out.map&&!s.out.utility,"map can open from equipment page");
        Check(click(11).editor==1&&!s.out.map&&s.out.wheel,"map can open original skill editor");
        click(11);click(12);Check(click(10).settings&&!s.out.wheel&&!MapWanted(s.in,s.out,s.in.at),"settings closes map demand");
    }
    for(int interruption=0;interruption<6;++interruption) {
        Sim s;s.in.wheel=true;s.in.hover=12;s.in.trigger=1;Check(s.Step().map,"map opened before interruption");
        switch(interruption) {
            case 0:s.in.wheel=false;break;
            case 1:s.in.cancel=true;break;
            case 2:s.in.tracked[0]=false;break;
            case 3:s.in.active=false;break;
            case 4:++s.in.session;break;
            case 5:++s.in.epoch;break;
        }
        s.Step();Check(!s.out.map&&!MapWanted(s.in,s.out,s.in.at)&&s.out.block&&s.out.select==-2&&s.out.request.action==Action::None,
            "closing, cancellation, tracking loss, native menu, session change and recenter cancel map and block held trigger");
        s.in.wheel=false;s.in.cancel=false;s.in.active=true;s.in.tracked[0]=true;s.in.trigger=0;s.Step(3);
        s.in.wheel=true;s.Step();Check(s.out.wheel&&!s.out.map&&s.e.Page()==MenuPage::Spells,"reopening after interruption starts on spells");
    }
    for(int i=0;i<8;++i){float a=i*Pi/4+.012f;
        Check(WheelCell(.5f+std::sin(a)*.28f,.45f-std::cos(a)*.28f,(i+7)%8)==(i+7)%8,"segment boundary has selection hysteresis");}
    Check(PotionType(Code('h','p','2'))==Potion::Health&&PotionType(Code('m','p','4'))==Potion::Mana,"potion categories");
    {
        Sim s;s.in.snapshot.belt[1]=s.in.snapshot.belt[0];s.in.snapshot.belt[1].items[0].handle=99;
        s.in.hand[0].p=s.out.belt[1].p+Vec{-.01f,0,0};s.in.grip[0]=1;
        Check(s.Step().bottle==1&&s.out.bottleItem.handle==99,"overlapping reach spheres grab the closest visible bottle");
    }
    {
        Sim s;s.in.wheel=true;s.in.hover=10;s.in.trigger=1;
        Check(s.Step().settings&&s.out.block&&!s.out.wheel&&s.out.request.action==Action::None,"settings takes ownership without a native command");
        Check(!s.Step(4).settings&&!s.out.wheel&&s.out.block,"held settings selection cannot repeat or cast");
        s.in.active=false;s.Step();s.in.active=true;
        Check(!s.Step().settings&&s.out.block,"return from settings requires trigger release");
    }
    {
        Sim s;s.in.wheel=true;s.in.hover=3;s.in.trigger=1;
        Check(s.Step().select==3&&s.out.block&&!s.out.wheel,"select closes wheel and consumes trigger");
        Check(s.Step(4).select==-2&&!s.out.wheel&&s.out.block,"held Y cannot reopen or cast");
        s.in.trigger=0;Check(!s.Step().block,"trigger release rearms casting");
        s.in.wheel=false;s.Step();s.in.wheel=true;Check(s.Step().wheel,"Y release permits new wheel");
    }
    {
        Sim s;s.in.wheel=true;s.in.hover=9;s.in.trigger=1;Check(s.Step().utility,"utility tab");
        s.in.trigger=0;s.Step();s.in.hover=1;s.in.trigger=1;
        const auto r=s.Step().request;Check(r.action==Action::Equip&&r.index==2,"desired set command");
        s.in.trigger=0;s.in.wheel=false;Check(s.Step().block,"swap blocks until confirmed");
        s.in.reply={r.sequence,10,Result::Applied};Check(s.Step().result==Result::Applied,"confirmed swap feedback");
    }
    {
        Sim s;s.in.hand[0].p=s.out.belt[0].p;s.Step();s.in.grip[0]=1;
        Check(s.Step().bottle==0&&s.out.request.action==Action::None,"physical bottle grab is non-consuming");
        s.in.hand[0].q={0,0,-.70710678f,.70710678f};
        // Neck touches the mouth with the right hand outside the face.
        s.in.hand[0].p={-.115f,1.6f,-.08f};
        int requests=0;Request request;
        for(int i=0;i<30;++i){auto o=s.Step();if(o.request.action==Action::Potion){++requests;request=o.request;}Check(o.block,"sip owns combat input");}
        Check(requests==1&&request.item.handle==42,"one drink of exact grabbed item");
        s.in.snapshot.belt[0].items[0]={43,2,Code('m','p','1')};s.in.snapshot.belt[0].items[1]={};++s.in.snapshot.belt[0].revision;
        s.in.reply={request.sequence,10,Result::Applied};
        Check(s.Step().bottleItem==request.item&&s.out.bottleEmpty,"confirmed consumption keeps grabbed identity across mixed-column refill");
        s.in.trigger=1;s.in.grip[0]=0;Check(s.Step().block,"held trigger after bottle drop stays blocked");
        auto snap=s.in.snapshot;snap.belt[0].items[0]={43,2,Code('m','p','1')};snap.belt[0].items[1]={};
        Check(PotionGone(snap,request),"authoritative removal can acknowledge native use");
    }
    {
        Sim s;auto scene=framevr::potions::MakeScene(s.in,s.out);
        Check(scene.count==1&&scene.items[0].code==Code('h','p','1'),"3D belt shows the next exact native potion, no bottles in empty columns");
        s.in.hand[0].p=s.out.belt[0].p;s.in.grip[0]=1;s.Step();
        scene=framevr::potions::MakeScene(s.in,s.out);
        Check(scene.count==1&&scene.items[0].pose.p.y>s.in.hand[0].p.y,"grab moves one bottle into the right grip, without a duplicate on the belt");
        Check(!scene.items[0].empty&&s.out.request.action==Action::None,"grabbing does not consume");
        // Hand at mouth is insufficient if the actual bottle opening misses.
        s.in.hand[0].p={0,1.6f,-.08f};s.in.hand[0].q={0,0,.70710678f,.70710678f};
        for(int i=0;i<20;++i)Check(s.Step().request.action==Action::None,"neck, not controller center, must reach the mouth");
        s.in.hand[0].p={.115f,1.6f,-.08f};s.Step(6);
        s.in.hand[0].p.z-=.2f;s.Step();s.in.hand[0].p.z+=.2f;
        for(int i=0;i<10;++i)Check(s.Step().request.action==Action::None,"leaving mouth resets sip dwell");
        const auto before=s.in.snapshot;
        Request request;for(int i=0;i<5;++i){s.Step();if(s.out.request.action!=Action::None)request=s.out.request;}
        Check(request.action==Action::Potion,"sustained bottle-neck contact requests a native drink");
        s.in.reply={request.sequence,s.in.session,Result::Refused};s.Step();
        Check(!s.out.bottleEmpty,"refused drink retains full bottle");
        for(int i=0;i<30;++i)Check(s.Step().request.action==Action::None,"refused drink never auto-retries");
        s.in.grip[0]=0;s.Step();scene=framevr::potions::MakeScene(s.in,s.out);
        Check(s.out.bottle<0&&scene.count==1&&s.in.snapshot.belt==before.belt,"release returns unconsumed bottle");
        s.in.active=false;s.Step();Check(framevr::potions::MakeScene(s.in,s.out).count==0,"menus hide physical bottles");
    }
    {
        Sim s;s.in.hand[0].p=s.out.belt[0].p;s.Step();s.in.grip[0]=1;s.Step();
        ++s.in.snapshot.belt[0].revision;s.in.hand[0].p={0,1.6f,-.08f};s.in.hand[0].q={0,0,1,0};
        for(int i=0;i<30;++i)Check(s.Step().request.action==Action::None&&s.out.bottle<0,"refill/reorder cancels captured bottle");
    }
    {
        Sim s;Config legacy;legacy.holsters=true;s.e.Configure(legacy);s.Step(3);s.in.hand[0].p=s.out.holster[1].p;s.Step();s.in.grip[0]=1;
        Check(s.Step().block&&s.out.request.action==Action::None,"holster requires deliberate pull");
        s.in.hand[0].p.z-=.15f;
        Check(s.Step().request.action==Action::Equip&&s.out.request.index==2,"holster pulls exact set");
        for(int i=0;i<10;++i)Check(s.Step().request.action==Action::None,"held holster does not spam toggles");
    }
    {
        Sim s;s.in.hand[0].p=s.out.belt[0].p;s.Step();s.in.grip[0]=1;s.Step();s.in.active=false;s.Step();
        s.in.active=true;s.in.hand[0].p={0,1.6f,-.08f};s.in.hand[0].q={0,0,1,0};
        for(int i=0;i<30;++i)Check(s.Step().request.action==Action::None,"tracking/menu return cannot consume held object");
    }
    {
        Sim s;s.in.wheel=true;s.in.hover=9;s.in.trigger=1;s.Step();s.in.trigger=0;s.Step();s.in.hover=4;s.in.trigger=1;
        Check(s.Step().request.action==Action::Potion,"native time and pose time can have unrelated epochs");
        ++s.in.epoch;Check(s.Step().cancelPending,"recenter invalidates queued native transaction");
        for(int i=0;i<12;++i)Check(s.Step().request.action==Action::None,"held selection cannot replay after recenter");
    }
    {
        Sim s;s.in.snapshot.vitalsKnown=true;s.in.snapshot.health=12;s.in.snapshot.maxHealth=100;
        Check(s.Step().critical&&s.out.feedbackHand==1,"critical health cue requests left-hand feedback");
        Check(!s.Step(90).critical,"critical cue is brief, not permanent HUD");
        s.in.snapshot.health=90;Check(!s.Step().critical,"healthy state removes cue");
    }
    {
        Sim s;s.in.hand[0].p=s.out.belt[0].p;s.Step();s.in.grip[0]=1;s.Step();
        s.in.hand[0].p.x=std::numeric_limits<float>::quiet_NaN();
        Check(s.Step().cancelPending&&s.out.bottle<0,"non-finite pose discards a held bottle and pending native work");
        s.in.hand[0].p={0,1.6f,-.08f};s.in.hand[0].q={0,0,1,0};
        for(int i=0;i<30;++i)Check(s.Step().request.action==Action::None,"tracking recovery with grip held cannot drink");
    }
    std::puts("Interaction ownership, wheel, holster, item identity and confirmation tests passed");
}
