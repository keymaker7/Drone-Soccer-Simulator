#include "DroneSoccerRules.h"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

using namespace drone_soccer;
namespace
{
int checks=0;
void Check(bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);}
bool Near(double a,double b){return std::abs(a-b)<1e-7;}
std::array<Vec3,10> Homes()
{
    std::array<Vec3,10> p{};
    for(int i=0;i<10;++i)p[i]={i<5?-6.5:6.5,-2.4+(i%5)*1.2,0.2};
    return p;
}
void At(MatchRules& m,std::array<Vec3,10>& p,int id,double x,double y=0,double z=3,double dt=.01)
{
    p[id]={x,y,z};m.Update(p,dt);
}
void Forward(MatchRules& m,std::array<Vec3,10>& p,int team)
{
    const int id=team*5;const double sign=team==0?1.0:-1.0;
    At(m,p,id,sign*6.1);At(m,p,id,sign*6.9);
}
void TestProfileAndPractice()
{
    Check(Near(RuleProfileFIDA40::Length,16)&&Near(RuleProfileFIDA40::Width,7)&&Near(RuleProfileFIDA40::Height,4.5),"Incorrect recommended FIDA arena");
    Check(Near(RuleProfileFIDA40::GoalX,6.5)&&Near(RuleProfileFIDA40::GoalHeight,3),"Incorrect legal goal position");
    Check(Near(RuleProfileFIDA40::GoalInnerRadius-RuleProfileFIDA40::BallRadius,.1),"Whole-ball aperture clearance missing");
    MatchRules m;auto p=Homes();
    Check(m.GetPhase()==Phase::Practice&&m.CurrentSet()==0&&m.IsArmingAllowed(),"Practice defaults incorrect");
    At(m,p,0,6.1);At(m,p,0,6.9);
    Check(m.Score(0)==1&&m.GetPhase()==Phase::Practice,"Practice scoring should use the same geometry without match clock");
}
void TestFullPassAndBounce()
{
    MatchRules m;auto p=Homes();At(m,p,0,6.1);At(m,p,0,6.5);
    Check(m.Score(0)==0,"Centre-plane crossing counted before entire ball cleared");
    At(m,p,0,6.77);Check(m.Score(0)==0,"Ball touching the rear hoop slab counted too early");
    At(m,p,0,6.79);Check(m.Score(0)==1&&m.IsOffside(0),"Complete forward transit should score and set team offside");
    At(m,p,0,7.2);Check(m.Score(0)==1,"Remaining behind goal scored twice");
    m.Reset();p=Homes();At(m,p,0,6.1);At(m,p,0,6.6);At(m,p,0,6.1);
    Check(m.Score(0)==0,"A ball bouncing back before fully exiting scored");
    m.Reset();p=Homes();At(m,p,0,6.5);At(m,p,0,6.9);
    Check(m.Score(0)==0,"First observation inside a hoop invented an entry");
    m.Reset();p=Homes();At(m,p,5,-6.1);At(m,p,5,-6.9);
    Check(m.Score(1)==1&&m.Score(0)==0,"Mirrored team scoring direction is wrong");
}
void TestRolesAndEdges()
{
    for(double y:{0.0,.099,.1,.101,.3})
    {
        MatchRules m;auto p=Homes();At(m,p,0,6.1,y);At(m,p,0,6.9,y);
        Check(m.Score(0)==(y<.1?1:0),"Sphere clearance or edge-grazing decision incorrect");
    }
    MatchRules m;auto p=Homes();At(m,p,0,6.1,0,3.101);At(m,p,0,6.9,0,3.101);
    Check(m.Score(0)==0,"Vertical ball surface intersected hoop but still scored");
    m.Reset();p=Homes();At(m,p,1,6.1);At(m,p,1,6.9);
    Check(m.Score(0)==0&&!m.IsOffside(0),"Non-striker scored or created striker offside");
    m.Reset();p=Homes();m.SetActive(0,false);At(m,p,0,6.1);At(m,p,0,6.9);
    Check(m.Score(0)==0,"Inactive striker scored");
}
void TestOffsideAndPenalties()
{
    MatchRules m;auto p=Homes();At(m,p,0,6.9);At(m,p,0,6.1);
    Check(m.Score(0)==0&&m.IsOffside(0),"Reverse opponent-goal passage must set offside without scoring");
    At(m,p,0,6.9);Check(m.Score(0)==0&&m.Penalties(0)==1,"Ignoring offside must not produce a consecutive score");
    p[1].x=.1;At(m,p,0,-.5);
    Check(m.IsOffside(0),"Striker alone returning incorrectly cleared team offside");
    p[1].x=-.1;m.Update(p,.01);
    Check(m.IsOffside(0),"A teammate's body still overlapping centre line cleared offside");
    p[1].x=-.21;m.Update(p,.01);Check(!m.IsOffside(0),"All five complete bodies retreated but offside remained");
    Forward(m,p,0);Check(m.Score(0)==1,"Goal after valid team retreat did not score");
    p[1].x=1;At(m,p,0,-.5);m.SetActive(1,false);m.Update(p,.01);
    Check(!m.IsOffside(0),"A formally inactive withdrawn player blocked active-team retreat");
    m.Reset();p=Homes();At(m,p,2,-6.9);At(m,p,2,-6.7);
    Check(m.Penalties(0)==1,"Partial reverse entry into own goal was not penalized");
    At(m,p,2,-6.65);Check(m.Penalties(0)==1,"A single reverse entry generated repeated penalties");
    At(m,p,2,-6.9);At(m,p,2,-6.7);Check(m.Penalties(0)==2,"A separate reverse entry was not tracked");
    m.Reset();p=Homes();At(m,p,2,-6.1);At(m,p,2,-6.9);
    Check(m.Penalties(0)==0,"Legal own-goal front-to-back transit was penalized");
}
void TestClockAndSetWins()
{
    MatchRules m;auto p=Homes();m.StartMatch();
    Check(m.GetPhase()==Phase::Ready&&Near(m.Remaining(),10)&&m.CurrentSet()==1&&!m.IsArmingAllowed(),"Ready gate incorrect");
    At(m,p,0,6.1);At(m,p,0,6.9);
    Check(m.Score(0)==0,"Ready phase allowed scoring");
    m.Update(p,m.Remaining());Check(m.GetPhase()==Phase::Playing&&Near(m.Remaining(),180)&&m.IsArmingAllowed(),"Playing did not start after ten seconds");
    At(m,p,0,6.1);At(m,p,0,6.9); // reverse then forward: reset offside by full team retreat first.
    m.StartMatch();p=Homes();m.Update(p,10);Forward(m,p,0);m.Update(p,m.Remaining());
    Check(m.SetWins(0)==1&&m.CurrentSet()==1&&m.GetPhase()==Phase::Break&&Near(m.Remaining(),300),"First set winner or maintenance interval incorrect");
    Check(!m.IsArmingAllowed(),"Maintenance break allowed arming");
    const int scored=m.Score(0);Forward(m,p,1);Check(m.Score(0)==scored&&m.Score(1)==0,"Break phase allowed goals");
    m.Update(p,m.Remaining());Check(m.CurrentSet()==2&&m.GetPhase()==Phase::Ready&&m.Score(0)==0,"New set did not clear set score");
    m.Update(p,10);p=Homes();m.Update(p,.01);Forward(m,p,0);m.Update(p,m.Remaining());
    Check(m.GetPhase()==Phase::Finished&&m.SetWins(0)==2&&m.CurrentSet()==2&&!m.IsArmingAllowed(),"Best-of-three match did not finish after two set wins");
    Forward(m,p,1);Check(m.Score(1)==0,"Finished match still scored goals");
    m.StartPractice();Check(m.GetPhase()==Phase::Practice&&m.SetWins(0)==0&&m.Penalties(0)==0,"Practice reset retained match sanctions or wins");
}
void TestDrawsCarryAndWhistle()
{
    MatchRules m;auto p=Homes();m.StartMatch();m.Update(p,300);
    Check(m.GetPhase()==Phase::Break&&Near(m.Remaining(),190)&&m.CurrentSet()==1,"Large dt lost state-transition carry");
    m.StartMatch();m.Update(p,1e12);
    Check(m.GetPhase()==Phase::Finished&&m.CurrentSet()==3&&m.SetWins(0)==0&&m.SetWins(1)==0,"Three draws should finish as a draw without an invented winner");
    m.StartMatch();p=Homes();m.Update(p,10);Forward(m,p,0);m.Update(p,m.Remaining());
    m.Update(p,300+10+180+300+10+180);
    Check(m.GetPhase()==Phase::Finished&&m.SetWins(0)==1&&m.SetWins(1)==0,"One set win plus two draws should preserve the winning set total");
    m.StartMatch();p=Homes();m.Update(p,10);At(m,p,0,6.1,0,3,179.9);At(m,p,0,6.9,0,3,1.0);
    Check(m.Score(0)==0&&m.GetPhase()==Phase::Break,"A full crossing after the end whistle was counted");
    m.StartMatch();p=Homes();m.Update(p,10);At(m,p,0,6.1,0,3,179.1);At(m,p,0,6.9,0,3,1.0);
    Check(m.Score(0)==1&&m.SetWins(0)==1,"A full crossing before the end whistle was discarded");
}
void TestInvalidInputsAndReset()
{
    MatchRules m;auto p=Homes();m.StartMatch();m.Update(p,-1);m.Update(p,0);m.Update(p,std::numeric_limits<double>::infinity());m.Update(p,std::numeric_limits<double>::quiet_NaN());
    Check(Near(m.Remaining(),10),"Invalid dt changed clock");
    p[0].x=std::numeric_limits<double>::quiet_NaN();m.Update(p,3);Check(Near(m.Remaining(),10),"Invalid pose changed clock");
    m.SetActive(-1,false);m.SetActive(10,false);Check(m.Score(-1)==0&&m.Penalties(3)==0&&!m.IsOffside(5)&&m.SetWins(-1)==0,"Invalid IDs caused an invalid result");
    m.Reset();p=Homes();Forward(m,p,0);Check(m.Score(0)==1,"Reset did not restore all active players");
    m.Reset();Check(m.Score(0)==0&&!m.IsOffside(0)&&m.Penalties(0)==0&&m.CurrentSet()==0,"Reset did not clear rule state");
}
void TestMovingGoalFrames()
{
    auto p=Homes();auto g=NominalGoalFrames();MatchRules m;
    g[1].center.y=.14;p[0]={6.1,.14,3};m.Update(p,.01,g);p[0].x=6.9;m.Update(p,.01,g);
    Check(m.Score(0)==1,"Translated hoop did not score its actual aperture");
    m.Reset();p=Homes();p[0]={6.1,0,3};m.Update(p,.01,g);p[0].x=6.9;m.Update(p,.01,g);
    Check(m.Score(0)==0,"Fixed nominal aperture scored outside the moved hoop");
    const double c=std::cos(.17),s=std::sin(.17);
    g=NominalGoalFrames();g[1].normal={c,s,0};g[1].side={-s,c,0};
    m.Reset();p=Homes();p[0]={6.5-.4*c,-.4*s,3};m.Update(p,.01,g);p[0]={6.5+.4*c,.4*s,3};m.Update(p,.01,g);
    Check(m.Score(0)==1,"Rotated hoop forward transit was not transformed into its local frame");
    g=NominalGoalFrames();g[1].normal={c,0,-s};g[1].up={s,0,c};
    m.Reset();p=Homes();p[0]={6.5-.4*c,0,3+.4*s};m.Update(p,.01,g);p[0]={6.5+.4*c,0,3-.4*s};m.Update(p,.01,g);
    Check(m.Score(0)==1,"Tilted hoop height and normal were not applied together");
    g=NominalGoalFrames();m.Reset();p=Homes();p[0]={6.5,0,3};g[1].center.x=6.9;m.Update(p,.01,g);g[1].center.x=6.1;m.Update(p,.01,g);
    Check(m.Score(0)==0,"A moving hoop swept over a stationary aircraft and created a goal");
    m.Reset();g=NominalGoalFrames();p=Homes();p[0]={6.1,0,3};m.Update(p,.01,g);g[1].normal={0,0,0};m.Update(p,.01,g);g=NominalGoalFrames();p[0].x=6.9;m.Update(p,.01,g);
    Check(m.Score(0)==0,"Invalid goal pose followed by recovery invented a transit");
}

void TestBothTeamsAllRoles()
{
    for(int team=0;team<2;++team)for(int role=0;role<5;++role)
    {
        MatchRules m;auto p=Homes();const int id=team*5+role;const double direction=team==0?1.0:-1.0;
        At(m,p,id,direction*6.1);At(m,p,id,direction*6.9);
        Check(m.Score(team)==(role==0?1:0),"One of the ten role/team combinations had incorrect scoring eligibility");
        Check(m.Score(1-team)==0,"A goal changed the opposing team's score");
        Check(m.IsOffside(team)==(role==0),"One of the ten role/team combinations had incorrect offside eligibility");
    }
}

void TestTransitIntegrity()
{
    MatchRules m;auto p=Homes();
    At(m,p,0,6.1,.08);At(m,p,0,6.9,-.08);
    Check(m.Score(0)==1,"Diagonal flight entirely inside the conservative aperture clearance did not score");
    m.Reset();p=Homes();At(m,p,0,6.1);At(m,p,0,6.5,.11);At(m,p,0,6.9);
    Check(m.Score(0)==0&&!m.IsOffside(0),"A transit leaving the opening halfway through became a goal");
    m.Reset();p=Homes();At(m,p,0,6.1);At(m,p,0,6.6);At(m,p,0,6.1);At(m,p,0,6.9);
    Check(m.Score(0)==1,"A cancelled bounce poisoned the next complete legal transit");
    for(int sample=0;sample<100;++sample)At(m,p,0,6.9+sample*.001);
    Check(m.Score(0)==1&&m.Penalties(0)==0,"Repeated samples behind the goal generated duplicate decisions");
    m.Reset();p=Homes();At(m,p,5,-6.9);
    Check(m.Score(1)==0&&!m.IsOffside(1),"Initial observation behind the red attack goal invented a passage");
    At(m,p,5,-6.1);At(m,p,5,-6.9);
    Check(m.Score(1)==0&&m.IsOffside(1)&&m.Penalties(1)==1,"Red repeated attack while offside did not mirror blue sanctions");
}

void TestBothTeamRetreatBoundaries()
{
    for(int team=0;team<2;++team)
    {
        MatchRules m;auto p=Homes();Forward(m,p,team);
        const int striker=team*5,teammate=striker+1;
        const double ownSide=team==0?-1.0:1.0;
        p[teammate].x=ownSide*RuleProfileFIDA40::BallRadius;
        At(m,p,striker,ownSide*.5);
        Check(m.IsOffside(team),"A team cleared offside while a teammate's body still touched the centre line");
        p[teammate].x=ownSide*(RuleProfileFIDA40::BallRadius+1e-10);m.Update(p,.01);
        Check(m.IsOffside(team),"Numerical noise at the centre line incorrectly cleared offside");
        p[teammate].x=ownSide*(RuleProfileFIDA40::BallRadius+.000001);m.Update(p,.01);
        Check(!m.IsOffside(team),"A complete mirrored team retreat failed to clear offside");
        Forward(m,p,team);
        Check(m.Score(team)==2&&m.Score(1-team)==0,"The next goal after a valid mirrored retreat did not increment only that team");
    }
}
void TestAIDuelRoles()
{
    for(int count:{3,4})
    {
        MatchRules m;auto p=Homes();for(int id=0;id<10;++id)m.SetActive(id,id==0||(id>=5&&id<5+count));
        Forward(m,p,0);Forward(m,p,1);Check(m.Score(0)==1&&m.Score(1)==1,"Both duel strikers must be eligible");
        for(int id=6;id<5+count;++id){At(m,p,id,-6.1);At(m,p,id,-6.9);Check(m.Score(1)==1,"Duel defender scored");}
        At(m,p,5,.5);Check(m.IsOffside(1),"AI teammates still across half must block next attack");
        for(int id=6;id<5+count;++id)p[id].x=.5;m.Update(p,.01);Check(!m.IsOffside(1),"Active AI team retreat must clear offside");
        Forward(m,p,1);Check(m.Score(1)==2,"Duel return did not permit second real rule goal");
        p[1].x=3;At(m,p,0,-.5);Check(!m.IsOffside(0),"Inactive blue roster must not block solo retreat");
    }
}

void TestAIPracticeRoles()
{
    for(int count:{3,4})
    {
        MatchRules m;auto p=Homes();for(int id=0;id<10;++id)m.SetActive(id,id==0||(id>=6&&id<6+count));
        Forward(m,p,0);Check(m.Score(0)==1&&m.IsOffside(0),"AI practice striker failed to score");
        Forward(m,p,1);Check(m.Score(1)==0,"Inactive AI striker scored");
        for(int id=6;id<6+count;++id){At(m,p,id,-6.1);At(m,p,id,-6.9);Check(m.Score(1)==0,"Defender scored without striker role");}
        p[1].x=3;At(m,p,0,-.5);Check(!m.IsOffside(0),"Inactive blue player blocked practice retreat");
        Forward(m,p,0);Check(m.Score(0)==2,"Second legal practice goal missing");
        m.Reset();p=Homes();Forward(m,p,1);Check(m.Score(1)==1,"Reset did not restore red striker");
    }
}

void Pass(MatchRules& m,std::array<Vec3,10>& p,int id)
{
    const double sign=id<5?1.0:-1.0;
    At(m,p,id,sign*6.1);At(m,p,id,sign*6.9);
}
void TestStrikerSuccession()
{
    MatchRules m;auto p=Homes();
    Check(m.Striker(0)==0&&m.Striker(1)==5&&m.IsStriker(0)&&m.IsStriker(5)&&!m.IsStriker(1)&&!m.IsStriker(6),"Original tagged Strikers incorrect");
    Check(m.Striker(-1)==-1&&m.Striker(2)==-1&&!m.IsStriker(-1)&&!m.IsStriker(10),"Invalid team or ID reported a Striker");
    Check(!m.ConfirmStrikerSuccession(-1)&&!m.ConfirmStrikerSuccession(2),"Invalid team accepted a succession");
    for(int team=0;team<2;++team)
    {
        m.Reset();p=Homes();const int striker=team*5,guide=striker+1;
        Check(m.ConfirmStrikerSuccession(team)&&m.Striker(team)==guide&&m.IsStriker(guide)&&!m.IsStriker(striker),"Referee-confirmed Guide succession failed");
        Check(m.Striker(1-team)==(1-team)*5,"Succession changed the opposing team's Striker");
        Pass(m,p,striker);Check(m.Score(team)==0&&!m.IsOffside(team),"Replaced Striker still scored or created offside");
        Pass(m,p,guide);Check(m.Score(team)==1&&m.IsOffside(team),"Successor Guide did not score with Striker rights");
        Check(!m.ConfirmStrikerSuccession(team)&&m.Striker(team)==guide,"A second succession was accepted without another tagged successor");
        Pass(m,p,(1-team)*5);Check(m.Score(1-team)==1,"Opposing original Striker lost scoring rights");
    }
    m.Reset();p=Homes();m.SetActive(1,false);
    Check(!m.ConfirmStrikerSuccession(0)&&m.Striker(0)==0,"An inactive Guide succeeded the Striker");
    m.Reset();p=Homes();At(m,p,1,6.1);At(m,p,1,6.5);m.ConfirmStrikerSuccession(0);At(m,p,1,6.9);
    Check(m.Score(0)==0&&!m.IsOffside(0),"A transit begun as Guide was completed as a Striker goal");
    // Return outside the opening so the way back is not a reverse passage that sets offside.
    At(m,p,1,6.1,1);At(m,p,1,6.1);At(m,p,1,6.9);
    Check(m.Score(0)==1&&m.Penalties(0)==0,"Successor could not score after starting a fresh transit");
    m.Reset();p=Homes();At(m,p,0,6.1);At(m,p,0,6.5);m.ConfirmStrikerSuccession(0);At(m,p,0,6.9);
    Check(m.Score(0)==0&&!m.IsOffside(0),"Replaced Striker completed a goal after succession");
    m.Reset();p=Homes();m.ConfirmStrikerSuccession(0);m.SetActive(1,false);Pass(m,p,1);
    Check(m.Score(0)==0,"Inactive successor scored");
    m.ConfirmStrikerSuccession(1);m.Reset();Check(m.Striker(0)==0&&m.Striker(1)==5,"Reset did not restore original Strikers");
    m.ConfirmStrikerSuccession(0);m.StartPractice();Check(m.Striker(0)==0,"Practice restart kept a succession");
}
void TestSuccessionAcrossPhases()
{
    MatchRules m;auto p=Homes();m.StartMatch();
    Check(!m.ConfirmStrikerSuccession(0)&&m.Striker(0)==0,"Succession accepted during the ready signal");
    m.Update(p,10);Check(m.GetPhase()==Phase::Playing&&m.ConfirmStrikerSuccession(0),"Succession rejected during live play");
    Pass(m,p,1);m.Update(p,m.Remaining());
    Check(m.GetPhase()==Phase::Break&&m.SetWins(0)==1&&m.Striker(0)==1,"Successor goal or role lost before the break");
    Check(!m.ConfirmStrikerSuccession(1)&&m.Striker(1)==5,"Succession accepted during the maintenance break");
    m.Update(p,m.Remaining());Check(m.CurrentSet()==2&&m.GetPhase()==Phase::Ready&&m.Striker(0)==0,"New set did not restore the original Striker");
    m.Update(p,10);p=Homes();m.Update(p,.01);Forward(m,p,0);m.Update(p,m.Remaining());
    Check(m.GetPhase()==Phase::Finished&&!m.ConfirmStrikerSuccession(0),"Succession accepted after the match finished");
}
}
int main()
{
    try
    {
        TestProfileAndPractice();TestFullPassAndBounce();TestRolesAndEdges();TestOffsideAndPenalties();
        TestClockAndSetWins();TestDrawsCarryAndWhistle();TestInvalidInputsAndReset();TestMovingGoalFrames();
        TestBothTeamsAllRoles();TestTransitIntegrity();TestBothTeamRetreatBoundaries();TestAIPracticeRoles();TestAIDuelRoles();
        TestStrikerSuccession();TestSuccessionAcrossPhases();
        std::cout<<"PASS: "<<checks<<" FIDA rule checks (whole-ball goals, roles, succession, offside, penalties, clock, sets and invalid inputs).\n";
        return 0;
    }
    catch(const std::exception& e){std::cerr<<"FAIL after "<<checks<<" checks: "<<e.what()<<"\n";return 1;}
}
