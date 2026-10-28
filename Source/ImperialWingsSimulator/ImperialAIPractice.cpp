#include "ImperialMatchManager.h"
#include "ImperialPlayerController.h"
#include "SoccerDronePawn.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"

bool AImperialMatchManager::StartAIPractice(ImperialAI::Settings Settings)
{
    if(!Settings.Valid()||!IsAutonomyAllowed()||IsAIPracticeActive())return false;
    const auto* Pilot=Cast<AImperialPlayerController>(UGameplayStatics::GetPlayerController(this,0));
    if(Pilot&&Pilot->IsReadOnlyView()){Log(TEXT("Select a pilot view before starting AI practice."));return false;}
    for(const auto& D:Drones)if(D->IsArmed()||D->GetActorLocation().Z>35||D->GetFlightSpeed()>.35)
    {Log(TEXT("Land and disarm every aircraft before starting AI practice."));return false;}
    // Explicit session start/reset: normal practice pads, no in-flight repositioning.
    ResetMatch();SelectDrone(0);AISettings=Settings;AIPlanner.Reset(Settings);
    AIAge=AILandingAge=0;bAILandingFault=false;Mode=EImperialMode::AIPractice;
    for(int32 I=0;I<Drones.Num();++I)Rules.SetActive(I,I==0||(I>=6&&I<6+Settings.Count));
    for(int32 I=0;I<Settings.Count;++I)
    {
        auto* D=GetDrone(6+I);D->SetEquipmentInspection(false);D->SetAutonomousEnabled(true);
        const auto P=AIPlanner.GetTargets()[I];D->SetAutonomousTarget(FVector(P.X,P.Y,P.Z)*100.);
    }
    Log(TEXT("AI defenders launching. Blue striker stays DISARMED: close menu, then arm manually at low throttle."));return true;
}
void AImperialMatchManager::StopAIPractice()
{
    if(Mode!=EImperialMode::AIPractice)return;
    Mode=EImperialMode::AILanding;AILandingAge=0;AILandingTargets.Reset();
    for(int32 I=0;I<AISettings.Count;++I)AILandingTargets.Add(GetDrone(6+I)->GetActorLocation());
    Log(TEXT("AI defenders landing vertically. Land your blue striker manually; RESET is an immediate session reset."));
}
void AImperialMatchManager::TickAIPractice(float Dt)
{
    if(!IsAIPracticeActive())return;
    if(!IsAutonomyAllowed()){for(int32 I=0;I<AISettings.Count;++I)GetDrone(6+I)->SetAutonomousEnabled(false);Mode=EImperialMode::Manual;return;}
    AIAge+=Dt;
    if(Mode==EImperialMode::AIPractice)
    {
        const FVector P=GetDrone(0)->GetActorLocation()/100.,V=GetDrone(0)->GetBodyMesh()->GetPhysicsLinearVelocity()/100.;
        // Bounded sub-intervals advance only the tactical target; the aircraft remains
        // controlled by the existing force/torque flight loop, not this planner clock.
        float Remaining=FMath::Clamp(Dt,0.f,.25f);
        while(Remaining>0){const float Step=FMath::Min(Remaining,.05f);AIPlanner.Step(Step,{P.X,P.Y,P.Z},{V.X,V.Y,V.Z});Remaining-=Step;}
        for(int32 I=0;I<AISettings.Count;++I){const auto T=AIPlanner.GetTargets()[I];GetDrone(6+I)->SetAutonomousTarget(FVector(T.X,T.Y,T.Z)*100.);}
        // A finite practice session avoids unattended battery depletion.
        if(AIAge>=180)StopAIPractice();
    }
    else
    {
        AILandingAge+=Dt;bool Landed=true;
        for(int32 I=0;I<AISettings.Count;++I)
        {
            auto* D=GetDrone(6+I);if(!D->IsAutonomousEnabled())continue;
            AILandingTargets[I].Z=FMath::Max(12.,AILandingTargets[I].Z-40.*FMath::Clamp(Dt,0.f,.25f));
            D->SetAutonomousTarget(AILandingTargets[I]);
            if(D->GetActorLocation().Z<32&&D->GetFlightSpeed()<.35)D->SetAutonomousEnabled(false);
            else Landed=false;
        }
        if(Landed){Mode=EImperialMode::Manual;for(int32 I=0;I<Drones.Num();++I)Rules.SetActive(I,true);Log(TEXT("AI practice ended. Defenders landed and disarmed; your manual control is retained."));}
        else if(AILandingAge>30&&!bAILandingFault){bAILandingFault=true;Log(TEXT("AI landing delayed by contact. Inspect the arena; RESET explicitly resets all aircraft."));}
    }
}
FString AImperialMatchManager::GetAIStatus() const
{
    const TCHAR* Level=AISettings.Difficulty==0?TEXT("BEGINNER"):(AISettings.Difficulty==1?TEXT("INTERMEDIATE"):TEXT("ADVANCED"));
    const TCHAR* Pattern=AIPlanner.GetPattern()==0?TEXT("STAGGERED"):(AIPlanner.GetPattern()==1?TEXT("PATROL"):TEXT("TRACKING"));
    return FString::Printf(TEXT("%s / %d DEFENDERS / SEED %u / %s / %.0f s"),Level,AISettings.Count,AISettings.Seed,IsAILanding()?TEXT("LANDING"):Pattern,AIAge);
}
