#pragma once
#include "AITraining.h"

namespace ImperialAI
{
enum class RecoveryAction { Track, Escape, Grounded, Unavailable, Failed };
// A bounded supervisor for existing motor-force control, not a new flight model.
class Recovery
{
public:
    void Reset(){*this=Recovery();}
    RecoveryAction Step(double Dt,Point Position,Point Target,double UpZ,bool Armed,bool Autonomous,int Slot)
    {
        if(!std::isfinite(Dt)||Dt<=0||Dt>.25)return Action;
        if(!Finite(Position)||!Finite(Target)||!std::isfinite(UpZ))return Action=RecoveryAction::Failed;
        if(Action==RecoveryAction::Grounded||Action==RecoveryAction::Unavailable||Action==RecoveryAction::Failed)return Action;
        Age+=Dt;
        GroundTime=Position.Z<.34&&UpZ<.35?GroundTime+Dt:0;
        MissingTime=Age>1&&(!Armed||!Autonomous)?MissingTime+Dt:0;
        TiltTime=Age>6&&UpZ<.2?TiltTime+Dt:0;
        if(GroundTime>=.4)return Action=RecoveryAction::Grounded;
        if(MissingTime>=.3)return Action=RecoveryAction::Unavailable;
        if(TiltTime>=2)return Action=RecoveryAction::Failed;
        if(Action==RecoveryAction::Escape)
        {
            EscapeTime+=Dt;
            if(EscapeTime>=3||(EscapeTime>=.6&&Distance(Position,EscapeTarget)<.3&&UpZ>.7))
            {Action=RecoveryAction::Track;WindowTime=0;Origin=Position;}
            return Action;
        }
        const double Error=Distance(Position,Target);
        ClearTime=Error<.45?ClearTime+Dt:0;
        if(ClearTime>=1)Consecutive=0;
        if(Age<6||Error<=.75){WindowTime=0;Origin=Position;return Action;}
        if(WindowTime==0||Distance(Position,Origin)>.22){WindowTime=0;Origin=Position;}
        WindowTime+=Dt;
        if(WindowTime<2)return Action;
        if(Consecutive>=2)return Action=RecoveryAction::Failed;
        ++Consecutive;++Attempts;EscapeTime=0;WindowTime=0;
        const double Side=Position.Y>.2?-1.:(Position.Y<-.2?1.:(Slot%2==0?1.:-1.));
        EscapeTarget={std::clamp(Position.X-.9,1.4,6.6),std::clamp(Position.Y+Side*.45,-2.8,2.8),std::clamp(Position.Z+.45,.65,3.7)};
        return Action=RecoveryAction::Escape;
    }
    Point GetTarget() const { return EscapeTarget; }
    int GetAttempts() const { return Attempts; }
    bool IsEscaping() const { return Action==RecoveryAction::Escape; }
private:
    RecoveryAction Action=RecoveryAction::Track;
    Point Origin{},EscapeTarget{};
    double Age=0,GroundTime=0,MissingTime=0,TiltTime=0,WindowTime=0,EscapeTime=0,ClearTime=0;
    int Consecutive=0,Attempts=0;
};
}
