#include "DroneSoccerRules.h"
#include <algorithm>
#include <cmath>

namespace drone_soccer
{
namespace
{
constexpr double Epsilon=1e-9;
bool ValidTeam(int team) { return team==0 || team==1; }
Vec3 Interpolate(Vec3 a,Vec3 b,double t)
{
    return {a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t,a.z+(b.z-a.z)*t};
}
bool Finite(Vec3 p) { return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z); }
double RadialSquared(Vec3 p)
{
    const double z=p.z-RuleProfileFIDA40::GoalHeight;
    return p.y*p.y+z*z;
}
bool Fits(Vec3 p)
{
    const double clearance=RuleProfileFIDA40::GoalInnerRadius-RuleProfileFIDA40::BallRadius;
    return RadialSquared(p)<clearance*clearance-Epsilon;
}
bool ClearSegment(Vec3 from,Vec3 to,double a,double b,double extent)
{
    if(std::abs(b-a)<Epsilon)return Fits(from)&&Fits(to);
    double t0=(-extent-a)/(b-a),t1=(extent-a)/(b-a);
    if(t0>t1)std::swap(t0,t1);
    t0=std::clamp(t0,0.0,1.0);t1=std::clamp(t1,0.0,1.0);
    return Fits(Interpolate(from,to,t0))&&Fits(Interpolate(from,to,t1));
}
}
MatchRules::MatchRules(){Reset();}
void MatchRules::ClearTransits(){for(auto& drone:transits)for(auto& ring:drone)ring=Transit{};}
void MatchRules::Reset()
{
    previous={};active.fill(true);scores={};penalties={};wins={};offside={};
    phase=Phase::Practice;currentSet=0;remaining=0;havePrevious=false;ClearTransits();
}
void MatchRules::StartPractice(){Reset();}
void MatchRules::StartMatch(){Reset();phase=Phase::Ready;currentSet=1;remaining=RuleProfileFIDA40::ReadySeconds;}
void MatchRules::SetActive(int id,bool value)
{
    if(id<0||id>=10)return;
    active[id]=value;
    if(!value)transits[id]={};
}
int MatchRules::Score(int team)const{return ValidTeam(team)?scores[team]:0;}
bool MatchRules::IsOffside(int team)const{return ValidTeam(team)&&offside[team];}
int MatchRules::Penalties(int team)const{return ValidTeam(team)?penalties[team]:0;}
int MatchRules::SetWins(int team)const{return ValidTeam(team)?wins[team]:0;}
void MatchRules::AdvanceClock(double dt)
{
    // A match contains at most three sets; twelve transitions cover even a huge dt.
    for(int transitions=0;transitions<12&&dt>0;++transitions)
    {
        if(phase==Phase::Practice||phase==Phase::Finished)return;
        const double consumed=std::min(dt,remaining);
        remaining=std::max(0.0,remaining-consumed);dt-=consumed;
        if(remaining>Epsilon)return;
        ClearTransits();
        if(phase==Phase::Ready){phase=Phase::Playing;remaining=RuleProfileFIDA40::SetSeconds;}
        else if(phase==Phase::Playing)
        {
            if(scores[0]>scores[1])++wins[0];else if(scores[1]>scores[0])++wins[1];
            if(wins[0]>=2||wins[1]>=2||currentSet>=3){phase=Phase::Finished;remaining=0;}
            else{phase=Phase::Break;remaining=RuleProfileFIDA40::BreakSeconds;}
        }
        else if(phase==Phase::Break)
        {
            ++currentSet;scores={};offside={};active.fill(true);
            phase=Phase::Ready;remaining=RuleProfileFIDA40::ReadySeconds;
        }
    }
}
void MatchRules::ProcessMotion(int id,int attackingTeam,Vec3 from,Vec3 to)
{
    const double sign=attackingTeam==0?1.0:-1.0;
    const double goal=sign*RuleProfileFIDA40::GoalX;
    const double a=sign*(from.x-goal),b=sign*(to.x-goal);
    const double extent=RuleProfileFIDA40::GoalThickness*0.5+RuleProfileFIDA40::BallRadius;
    Transit& transit=transits[id][attackingTeam];
    const bool ownRing=id/5!=attackingTeam;
    if(transit.direction==0)
    {
        if(a<=-extent&&b>-extent){transit.direction=1;transit.clear=true;}
        else if(a>=extent&&b<extent)
        {
            transit.direction=-1;transit.clear=true;
            // FIDA prohibits even partial reverse entry through one's own hoop.
            const double t=(extent-a)/(b-a);
            if(ownRing&&Fits(Interpolate(from,to,std::clamp(t,0.0,1.0))))++penalties[id/5];
        }
    }
    if(transit.direction==0)return;
    transit.clear=transit.clear&&ClearSegment(from,to,a,b,extent);
    const bool complete=transit.direction==1?b>=extent:b<=-extent;
    const bool withdrew=transit.direction==1?b<=-extent:b>=extent;
    if(complete)
    {
        if(transit.clear&&!ownRing&&(id==0||id==5))
        {
            const int team=id/5;
            if(transit.direction==1)
            {
                if(!offside[team])++scores[team];
                else ++penalties[team];
            }
            offside[team]=true;
        }
        transit=Transit{};
    }
    else if(withdrew)transit=Transit{};
}
void MatchRules::Update(const std::array<Vec3,10>& positions,double dt)
{
    if(!std::isfinite(dt)||dt<=0)return;
    for(const Vec3 p:positions)if(!Finite(p))return;
    const bool mayScore=phase==Phase::Practice||phase==Phase::Playing;
    // A frame spanning the end whistle may only score in its playing portion.
    const double fraction=phase==Phase::Playing?std::min(1.0,remaining/dt):1.0;
    if(havePrevious&&mayScore)
    {
        for(int id=0;id<10;++id)if(active[id])
            for(int team=0;team<2;++team)ProcessMotion(id,team,previous[id],Interpolate(previous[id],positions[id],fraction));
        for(int team=0;team<2;++team)if(offside[team])
        {
            bool retreated=true;
            for(int id=team*5;id<team*5+5;++id)if(active[id])
            {
                const double x=Interpolate(previous[id],positions[id],fraction).x;
                if(team==0?x+RuleProfileFIDA40::BallRadius>=0:x-RuleProfileFIDA40::BallRadius<=0)retreated=false;
            }
            if(retreated)offside[team]=false;
        }
    }
    previous=positions;havePrevious=true;AdvanceClock(dt);
}
}
