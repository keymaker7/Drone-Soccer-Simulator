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
// Role slots match ASoccerDronePawn: 0 Striker, 1 Guide, 2 Libero, 3 Sweeper, 4 Keeper.
constexpr std::array<int,2> OriginalStrikers{0,5};
int GuideId(int team) { return team*5+1; }
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
std::array<GoalFrame,2> NominalGoalFrames(){std::array<GoalFrame,2> G{};G[0].center={-6.5,0,3};G[1].center={6.5,0,3};return G;}
void MatchRules::ClearTransits(){for(auto& drone:transits)for(auto& ring:drone)ring=Transit{};}
void MatchRules::Reset()
{
    previous={};previousGoals=NominalGoalFrames();active.fill(true);scores={};penalties={};wins={};offside={};
    strikers=OriginalStrikers;phase=Phase::Practice;currentSet=0;remaining=0;havePrevious=false;ClearTransits();
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
int MatchRules::Striker(int team)const{return ValidTeam(team)?strikers[team]:-1;}
bool MatchRules::IsStriker(int id)const{return id>=0&&id<10&&strikers[id/5]==id;}
bool MatchRules::ConfirmStrikerSuccession(int team)
{
    // Only the referee's confirmation is modelled. Declaring the Striker unable,
    // stopping it and the Guide's legal contact happen before this call; a
    // collision alone must never reach it. Only the Guide carries the second tag,
    // so a team can succeed once per set, and only while play is live.
    if(!ValidTeam(team)||!(phase==Phase::Practice||phase==Phase::Playing))return false;
    const int guide=GuideId(team);
    if(strikers[team]!=OriginalStrikers[team]||!active[guide])return false;
    strikers[team]=guide;
    // A Guide already partway through the attack hoop must start a new transit.
    transits[guide][team]=Transit{};
    return true;
}
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
            // Simulator choice: each set restarts with the original tagged Striker,
            // like the full roster restored below; not a confirmed FIDA clause.
            ++currentSet;scores={};offside={};active.fill(true);strikers=OriginalStrikers;
            phase=Phase::Ready;remaining=RuleProfileFIDA40::ReadySeconds;
        }
    }
}
void MatchRules::ProcessMotion(int id,int attackingTeam,Vec3 from,Vec3 to,bool bodyMoved)
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
            if(bodyMoved&&ownRing&&Fits(Interpolate(from,to,std::clamp(t,0.0,1.0))))++penalties[id/5];
        }
    }
    if(transit.direction==0)return;
    transit.clear=transit.clear&&ClearSegment(from,to,a,b,extent);
    const bool complete=transit.direction==1?b>=extent:b<=-extent;
    const bool withdrew=transit.direction==1?b<=-extent:b>=extent;
    if(complete)
    {
        if(bodyMoved&&transit.clear&&!ownRing&&IsStriker(id))
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
{Update(positions,dt,NominalGoalFrames());}
void MatchRules::Update(const std::array<Vec3,10>& positions,double dt,const std::array<GoalFrame,2>& goals)
{
    if(!std::isfinite(dt)||dt<=0)return;
    for(const Vec3 p:positions)if(!Finite(p))return;
    const auto Dot=[](Vec3 A,Vec3 B){return A.x*B.x+A.y*B.y+A.z*B.z;};
    const auto Sub=[](Vec3 A,Vec3 B){return Vec3{A.x-B.x,A.y-B.y,A.z-B.z};};
    const auto Unit=[&](Vec3 V,Vec3 Fallback){const double L=std::sqrt(Dot(V,V));return L>1e-8?Vec3{V.x/L,V.y/L,V.z/L}:Fallback;};
    const auto Cross=[](Vec3 A,Vec3 B){return Vec3{A.y*B.z-A.z*B.y,A.z*B.x-A.x*B.z,A.x*B.y-A.y*B.x};};
    for(const auto& G:goals)
    {
        if(!Finite(G.center)||!Finite(G.normal)||!Finite(G.side)||!Finite(G.up)||
            std::abs(Dot(G.normal,G.normal)-1)>1e-4||std::abs(Dot(G.side,G.side)-1)>1e-4||std::abs(Dot(G.up,G.up)-1)>1e-4||
            std::abs(Dot(G.normal,G.side))>1e-4||Dot(Cross(G.normal,G.side),G.up)<.9999)
        {havePrevious=false;ClearTransits();AdvanceClock(dt);return;}
    }
    const auto Canonical=[&](Vec3 P,const GoalFrame& G,int Index){const Vec3 D=Sub(P,G.center);return Vec3{(Index==0?-6.5:6.5)+Dot(D,G.normal),Dot(D,G.side),3+Dot(D,G.up)};};
    const bool mayScore=phase==Phase::Practice||phase==Phase::Playing;
    // A frame spanning the end whistle may only score in its playing portion.
    const double fraction=phase==Phase::Playing?std::min(1.0,remaining/dt):1.0;
    if(havePrevious&&mayScore)
    {
        for(int id=0;id<10;++id)if(active[id])for(int team=0;team<2;++team)
        {
            const int Index=1-team;GoalFrame End=goals[Index];
            if(fraction<1)
            {
                const auto& Start=previousGoals[Index];End.center=Interpolate(Start.center,End.center,fraction);
                End.normal=Unit(Interpolate(Start.normal,End.normal,fraction),Start.normal);
                Vec3 Side=Interpolate(Start.side,End.side,fraction);const double Projection=Dot(Side,End.normal);
                End.side=Unit({Side.x-Projection*End.normal.x,Side.y-Projection*End.normal.y,Side.z-Projection*End.normal.z},Start.side);End.up=Cross(End.normal,End.side);
            }
            const Vec3 Last=Interpolate(previous[id],positions[id],fraction);
            const bool BodyMoved=std::abs(Dot(Sub(Last,previous[id]),End.normal))>Epsilon;
            ProcessMotion(id,team,Canonical(previous[id],previousGoals[Index],Index),Canonical(Last,End,Index),BodyMoved);
        }
        for(int team=0;team<2;++team)if(offside[team])
        {
            bool retreated=true;
            for(int id=team*5;id<team*5+5;++id)if(active[id])
            {
                const double x=Interpolate(previous[id],positions[id],fraction).x;
                // Interpolation can place an exactly touching body a fraction
                // of a floating-point unit across the line. Require positive
                // clearance before lifting offside, symmetrically for both teams.
                if(team==0?x+RuleProfileFIDA40::BallRadius>=-Epsilon:x-RuleProfileFIDA40::BallRadius<=Epsilon)retreated=false;
            }
            if(retreated)offside[team]=false;
        }
    }
    previous=positions;previousGoals=goals;havePrevious=true;AdvanceClock(dt);
}
}
