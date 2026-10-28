#include "ImperialAIDuelVerifier.h"
#include "ImperialMatchManager.h"
#include "ImperialPlayerController.h"
#include "ImperialHUD.h"
#include "ImperialBuildInfo.h"
#include "SoccerDronePawn.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/FileHelper.h"
#include "Engine/Engine.h"
#include "UnrealClient.h"
#include "HAL/PlatformMisc.h"
AImperialAIDuelVerifier::AImperialAIDuelVerifier(){PrimaryActorTick.bCanEverTick=true;PrimaryActorTick.TickGroup=TG_PostPhysics;}
void AImperialAIDuelVerifier::Check(const TCHAR* Name,bool Passed,double Value)
{auto R=MakeShared<FJsonObject>();R->SetNumberField(TEXT("case"),Case);R->SetStringField(TEXT("check"),Name);R->SetBoolField(TEXT("ok"),Passed);R->SetNumberField(TEXT("value"),Value);Checks.Add(MakeShared<FJsonValueObject>(R));bOk&=Passed;}
void AImperialAIDuelVerifier::Finish()
{
    auto R=MakeShared<FJsonObject>();R->SetBoolField(TEXT("ok"),bOk);R->SetArrayField(TEXT("checks"),Checks);R->SetArrayField(TEXT("samples"),Samples);R->SetObjectField(TEXT("build"),ImperialBuildInfo::Json());R->SetNumberField(TEXT("min_game_dt_s"),MinDt);R->SetNumberField(TEXT("max_game_dt_s"),MaxDt);
    FString File,Json;FParse::Value(FCommandLine::Get(),TEXT("ImperialAIDuelResult="),File);FJsonSerializer::Serialize(R,TJsonWriterFactory<>::Create(&Json));const bool Saved=FFileHelper::SaveStringToFile(Json,*File,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);SetActorTickEnabled(false);FPlatformMisc::RequestExitWithStatus(false,bOk&&Saved?0:1);
}
void AImperialAIDuelVerifier::Tick(float Dt)
{
    Super::Tick(Dt);Age+=Dt;Total+=Dt;MinDt=FMath::Min(MinDt,static_cast<double>(Dt));MaxDt=FMath::Max(MaxDt,static_cast<double>(Dt));
    if(Total>1500){Check(TEXT("bounded duel fixture"),false);Finish();return;}
    auto* M=Cast<AImperialMatchManager>(UGameplayStatics::GetActorOfClass(GetWorld(),AImperialMatchManager::StaticClass()));auto* PC=Cast<AImperialPlayerController>(UGameplayStatics::GetPlayerController(this,0));auto* H=PC?Cast<AImperialHUD>(PC->GetHUD()):nullptr;if(!M||!H||Age<.25)return;
    const bool Interrupted=FParse::Param(FCommandLine::Get(),TEXT("ImperialAIDuelInterrupted"));
    auto* Player=M->GetDrone(0);auto* Striker=M->GetDrone(5);const int32 Count=Case<3?3:4;
    if(Stage==0)
    {
        if(FParse::Param(FCommandLine::Get(),TEXT("ImperialAIDuelReload")))
        {
            const auto& R=M->GetAIResult();Check(TEXT("saved duel restores scores roster and winner"),R.Exists()&&R.bSaved&&R.Settings.Duel&&R.OpponentGoals>=2&&R.Goals==0&&R.Aircraft.Num()==R.Settings.Count+1&&FString(R.Outcome())==TEXT("RED WINS"));
            bool Safe=!M->IsAIPracticeActive();for(const auto& D:M->GetDrones())Safe&=!D->IsArmed();Check(TEXT("loading a duel record never launches aircraft"),Safe);Finish();return;
        }
        FImperialAIResult Result;Result.Settings.Duel=true;Result.StopReason=TEXT("time_limit");Result.Goals=2;Result.OpponentGoals=1;
        Check(TEXT("winner comparison uses both teams"),FString(Result.Outcome())==TEXT("BLUE WINS"));Result.OpponentGoals=2;Check(TEXT("equal score is a draw"),FString(Result.Outcome())==TEXT("DRAW"));Result.StopReason=TEXT("manual_stop");Check(TEXT("early stop cannot award a win"),FString(Result.Outcome())==TEXT("INTERRUPTED"));
        FParse::Value(FCommandLine::Get(),TEXT("ImperialAIDuelCapture="),Capture);FParse::Value(FCommandLine::Get(),TEXT("ImperialAIDuelCases="),CaseCount);CaseCount=FMath::Clamp(CaseCount,1,6);if(GEngine)GEngine->SetMaxFPS(Capture.IsEmpty()?0:60);
        M->ResetMatch();M->ChangeView(1);if(H->IsRadioPanelOpen())H->ToggleRadioPanel();if(!H->IsPilotMenuOpen())H->ToggleAIPracticePanel();
        H->NotifyHitBoxClick(TEXT("AI_Duel"));H->NotifyHitBoxClick(Count==3?TEXT("AI_Count3"):TEXT("AI_Count4"));H->NotifyHitBoxClick(Case%3==0?TEXT("AI_Easy"):(Case%3==1?TEXT("AI_Medium"):TEXT("AI_Hard")));H->NotifyHitBoxClick(TEXT("AI_Start"));
        Check(TEXT("native menu selects duel and launches only AI roster"),M->IsAIPracticeActive()&&M->GetAISettings().Duel&&M->GetAISettings().Count==Count&&M->GetAISettings().Difficulty==Case%3&&!Player->IsArmed());
        M->SelectDrone(5);Check(TEXT("AI striker possession rejected"),M->GetSelected()==0);M->StartMatch();Check(TEXT("official match cannot replace active AI duel"),M->IsAIPracticeActive()&&M->IsAutonomyAllowed());
        if(!M->IsAIPracticeActive()){Finish();return;}
        if(Case==0&&!M->IsRecordingFlight())M->ToggleFlightRecording();
        bPlayerSafe=bRosterSafe=bFinite=true;bSawRetreat=false;LastSample=-1;Stage=1;Age=0;return;
    }
    if(Stage==1)
    {
        bPlayerSafe&=!Player->IsArmed()&&!Player->IsAutonomousEnabled()&&M->GetSelected()==0;
        for(int32 I=0;I<10;++I){auto* D=M->GetDrone(I);bFinite&=!D->GetActorLocation().ContainsNaN()&&D->GetFlightSpeed()<12;if(I<5||I>=5+Count)bRosterSafe&=!D->IsArmed()&&!D->IsAutonomousEnabled();}
        bSawRetreat|=M->IsOffside(1);if(Age-LastSample>=1)
        {
            LastSample=Age;auto S=MakeShared<FJsonObject>();const FVector P=Striker->GetActorLocation()/100.,T=Striker->GetAutonomousTarget()/100.;S->SetNumberField(TEXT("case"),Case);S->SetNumberField(TEXT("time"),Age);S->SetNumberField(TEXT("phase"),M->GetAIAttackPhase());S->SetNumberField(TEXT("cycles"),M->GetAIAttackCycles());S->SetNumberField(TEXT("red"),M->GetScore(1));S->SetNumberField(TEXT("attempts"),M->GetAIRecoveryAttempts());S->SetNumberField(TEXT("tilt"),Striker->GetTiltDegrees());S->SetArrayField(TEXT("position"),{MakeShared<FJsonValueNumber>(P.X),MakeShared<FJsonValueNumber>(P.Y),MakeShared<FJsonValueNumber>(P.Z)});S->SetArrayField(TEXT("target"),{MakeShared<FJsonValueNumber>(T.X),MakeShared<FJsonValueNumber>(T.Y),MakeShared<FJsonValueNumber>(T.Z)});Samples.Add(MakeShared<FJsonValueObject>(S));
        }
        if(Interrupted&&Age>=8&&M->IsAIPracticeActive()&&!M->IsAILanding())M->StopAIPractice();
        if(M->IsAIPracticeActive()&&Age<215)return;
        Check(TEXT("bounded session stop lands all AI"),!M->IsAIPracticeActive()&&(Interrupted?(Age>=8&&Age<35):(Age>=180&&Age<215)),Age);
        const auto& R=M->GetAIResult();if(!Interrupted)Check(TEXT("AI striker physically scores twice with own-half retreat"),M->GetScore(1)>=2&&M->GetAIAttackCycles()>=1&&bSawRetreat,M->GetScore(1));
        Check(TEXT("player ownership and inactive roster preserved"),bPlayerSafe&&bRosterSafe&&bFinite);
        bool Landed=true;for(int32 I=5;I<5+Count;++I)Landed&=!M->GetDrone(I)->IsArmed()&&M->GetDrone(I)->GetAltitudeMeters()<.32;
        Check(TEXT("all AI physically on floor with motors stopped"),Landed);
        Check(TEXT("duel result has real score winner time limit and landing"),R.bSaved&&R.bTestSession&&R.Settings.Duel&&R.Settings.Count==Count&&R.Goals==0&&R.OpponentGoals==M->GetScore(1)&&R.StopReason==(Interrupted?TEXT("manual_stop"):TEXT("time_limit"))&&R.CompletionReason==TEXT("landed")&&FString(R.Outcome())==(Interrupted?TEXT("INTERRUPTED"):TEXT("RED WINS")));
        if(M->IsRecordingFlight())M->ToggleFlightRecording();
        ScoreAtStop=M->GetScore(1);Stage=2;Age=0;
        // Explicit post-finish diagnostic flight: any later physical crossing must not score.
        Player->ResetDrone();Player->GetBodyMesh()->SetWorldLocationAndRotation(FVector(530,0,300),FRotator::ZeroRotator,false,nullptr,ETeleportType::TeleportPhysics);Player->SetAutonomousEnabled(true);Player->SetAutonomousTarget(FVector(730,0,300));return;
    }
    if(Stage==2&&Age>12)
    {
        Check(TEXT("score remains frozen during a real post-finish crossing"),M->IsAIDuelFinished()&&Player->GetActorLocation().X>678&&M->GetScore(0)==0&&M->GetScore(1)==ScoreAtStop);
        Player->SetAutonomousEnabled(false);M->ResetMatch();Check(TEXT("explicit reset reopens normal training and clears scores"),!M->IsAIDuelFinished()&&M->GetScore(0)==0&&M->GetScore(1)==0);
        if(!Capture.IsEmpty()){H->ToggleAIPracticePanel();H->NotifyHitBoxClick(TEXT("AI_Last"));FScreenshotRequest::RequestScreenshot(Capture,true,false);Stage=9;Age=0;return;}
        if(++Case>=CaseCount){Finish();return;}Stage=0;Age=0;return;
    }
    if(Stage==9&&Age>1)Finish();
}
