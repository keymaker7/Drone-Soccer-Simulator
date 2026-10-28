#include "ImperialAIResultsVerifier.h"
#include "ImperialMatchManager.h"
#include "ImperialPlayerController.h"
#include "ImperialHUD.h"
#include "ImperialBuildInfo.h"
#include "ImperialUserData.h"
#include "SoccerDronePawn.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "HAL/FileManager.h"
#include "Engine/Engine.h"
#include "UnrealClient.h"
#include "HAL/PlatformMisc.h"
AImperialAIResultsVerifier::AImperialAIResultsVerifier(){PrimaryActorTick.bCanEverTick=true;PrimaryActorTick.TickGroup=TG_PostPhysics;}
void AImperialAIResultsVerifier::Check(const TCHAR* Name,bool Passed)
{auto O=MakeShared<FJsonObject>();O->SetStringField(TEXT("check"),Name);O->SetBoolField(TEXT("ok"),Passed);Checks.Add(MakeShared<FJsonValueObject>(O));bOk&=Passed;}
void AImperialAIResultsVerifier::Finish()
{
    auto O=MakeShared<FJsonObject>();O->SetBoolField(TEXT("ok"),bOk);O->SetArrayField(TEXT("checks"),Checks);O->SetObjectField(TEXT("build"),ImperialBuildInfo::Json());O->SetStringField(TEXT("case"),Case);
    FString Json,File;FParse::Value(FCommandLine::Get(),TEXT("ImperialAIResultsResult="),File);FJsonSerializer::Serialize(O,TJsonWriterFactory<>::Create(&Json));
    const bool Saved=FFileHelper::SaveStringToFile(Json,*File,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);SetActorTickEnabled(false);FPlatformMisc::RequestExitWithStatus(false,bOk&&Saved?0:1);
}
void AImperialAIResultsVerifier::Tick(float Dt)
{
    Super::Tick(Dt);Age+=Dt;Total+=Dt;if(Total>75){Check(TEXT("bounded scenario"),false);Finish();return;}
    auto* M=Cast<AImperialMatchManager>(UGameplayStatics::GetActorOfClass(GetWorld(),AImperialMatchManager::StaticClass()));
    auto* PC=Cast<AImperialPlayerController>(UGameplayStatics::GetPlayerController(this,0));auto* H=PC?Cast<AImperialHUD>(PC->GetHUD()):nullptr;
    if(!M||!H||Age<.3)return;auto* D=M->GetDrone(0);
    if(Stage==0)
    {
        FParse::Value(FCommandLine::Get(),TEXT("ImperialAIResultsCase="),Case);FParse::Value(FCommandLine::Get(),TEXT("ImperialAIResultsCapture="),Capture);
        if(GEngine)GEngine->SetMaxFPS(Capture.IsEmpty()?0:60);
        if(Case==TEXT("reload")||Case==TEXT("invalid"))
        {
            const auto& R=M->GetAIResult();Check(TEXT("loading matches expected valid-report availability"),R.Exists()==(Case==TEXT("reload")));
            if(R.Exists()){Check(TEXT("persisted one-goal actual-flight report restored"),R.Goals==1&&R.Settings.Count==3&&R.bSaved&&R.CompletionReason==TEXT("landed"));FirstId=R.SessionId;M->ResetMatch();Check(TEXT("ordinary reset preserves finished report"),M->GetAIResult().SessionId==FirstId);}
            Check(TEXT("loading never arms or starts AI"),!D->IsArmed()&&!M->IsAIPracticeActive());
            if(!Capture.IsEmpty()&&R.Exists()){H->ToggleAIPracticePanel();H->NotifyHitBoxClick(TEXT("AI_Last"));FScreenshotRequest::RequestScreenshot(Capture,true,false);Stage=9;Age=0;return;}Finish();return;
        }
        if(Case==TEXT("savefail")||Case==TEXT("discard"))
        {
            Check(TEXT("failure test starts AI"),M->StartAIPractice({3,1,52}));Stage=7;Age=0;return;
        }
        if(Case==TEXT("exit")){Check(TEXT("exit test starts AI"),M->StartAIPractice({4,1,555}));Stage=10;Age=0;return;}
        Check(TEXT("fresh user has no invented result"),!M->GetAIResult().Exists());Check(TEXT("practice starts"),M->StartAIPractice({3,0,1}));
        // Explicit fixture initial placement, then only the existing force-based
        // autopilot moves the striker through the goal. Not a human/USB flight.
        D->ResetDrone();D->GetBodyMesh()->SetWorldLocationAndRotation(FVector(590,0,300),FRotator::ZeroRotator,false,nullptr,ETeleportType::TeleportPhysics);
        D->SetAutonomousEnabled(true);D->SetAutonomousTarget(FVector(590,0,300));Stage=1;Age=0;return;
    }
    if(Stage==1&&Age>4){D->SetAutonomousTarget(FVector(725,0,300));Stage=2;Age=0;return;}
    if(Stage==2)
    {
        if(M->GetScore(0)==1){Check(TEXT("physical whole-ball goal accepted"),D->GetActorLocation().X>678);D->SetAutonomousEnabled(false);Stage=3;Age=0;}
        else if(Age>15){Check(TEXT("physical goal reached within bound"),false);M->ResetMatch();Finish();}return;
    }
    if(Stage==3&&Age>6)
    {
        ExpectedImpacts=D->GetGuardImpactCount();Check(TEXT("physical fall registers guard impacts"),ExpectedImpacts>0);
        M->StopAIPractice();Stage=4;Age=0;return;
    }
    if(Stage==4&&!M->IsAIPracticeActive())
    {
        const auto& R=M->GetAIResult();FirstId=R.SessionId;
        Check(TEXT("finished session stores authoritative goal and timing"),R.Exists()&&R.bSaved&&R.Goals==1&&R.PracticeSeconds>10&&R.PracticeSeconds<40&&R.LandingSeconds>0&&R.LandingSeconds<30);
        Check(TEXT("manual stop and completed landing distinguished"),R.StopReason==TEXT("manual_stop")&&R.CompletionReason==TEXT("landed")&&R.bTestSession);
        Check(TEXT("per-aircraft guard snapshot matches real contact counter"),R.Aircraft.Num()==4&&R.Aircraft[0].Id==0&&R.Aircraft[0].GuardImpacts==ExpectedImpacts);
        Check(TEXT("saving again is idempotent"),M->SaveAIResult()&&M->GetAIResult().SessionId==FirstId);
        H->ToggleAIPracticePanel();H->NotifyHitBoxClick(TEXT("AI_Last"));H->NotifyHitBoxClick(TEXT("AI_Start"));H->NotifyHitBoxClick(TEXT("ArmSelected"));
        Check(TEXT("result page blocks flight and session-start clicks"),!D->IsArmed()&&!M->IsAIPracticeActive());H->NotifyHitBoxClick(TEXT("AI_Close"));
        if(!Capture.IsEmpty()){H->ToggleAIPracticePanel();H->NotifyHitBoxClick(TEXT("AI_Last"));FScreenshotRequest::RequestScreenshot(Capture,true,false);Stage=9;Age=0;return;}
        // Keep this one-goal report as the reload fixture. Interrupted sessions
        // are tested in a separate user directory to avoid changing latest.
        Finish();return;
    }
    if(Stage==7&&Age>1)
    {
        M->ResetMatch();const auto& R=M->GetAIResult();FirstId=R.SessionId;
        Check(TEXT("failed save preserves interrupted result in memory"),R.Exists()&&!R.bSaved&&R.StopReason==TEXT("reset")&&R.CompletionReason==TEXT("reset")&&R.PracticeSeconds>1);
        Check(TEXT("unsaved result cannot silently be overwritten by new session"),!M->StartAIPractice({3,0,1})&&M->GetAIResult().SessionId==FirstId);
        if(Case==TEXT("discard"))
        {
            H->ToggleAIPracticePanel();H->NotifyHitBoxClick(TEXT("AI_Last"));H->NotifyHitBoxClick(TEXT("AI_ResultDiscard"));
            Check(TEXT("explicit discard clears only unsaved in-memory result"),!M->GetAIResult().Exists());H->NotifyHitBoxClick(TEXT("AI_Close"));
            Check(TEXT("explicit discard permits a new session"),M->StartAIPractice({3,0,1}));M->ResetMatch();M->DiscardUnsavedAIResult();Finish();return;
        }
        const FString Block=ImperialUserData::Path(TEXT("TrainingResults"));
        Check(TEXT("fixture removes only its own blocking file"),IFileManager::Get().Delete(*Block));
        H->ToggleAIPracticePanel();H->NotifyHitBoxClick(TEXT("AI_Last"));H->NotifyHitBoxClick(TEXT("AI_ResultSave"));
        Check(TEXT("retry saves the original session without changing ID"),M->GetAIResult().bSaved&&M->GetAIResult().SessionId==FirstId);
        H->NotifyHitBoxClick(TEXT("AI_ResultDiscard"));Check(TEXT("discard cannot delete a saved result"),M->GetAIResult().SessionId==FirstId);H->NotifyHitBoxClick(TEXT("AI_Close"));
        Check(TEXT("new practice starts after successful retry"),M->StartAIPractice({4,2,17}));Stage=8;Age=0;return;
    }
    if(Stage==8&&Age>1)
    {
        M->StopAIPractice();M->ResetMatch();const auto& R=M->GetAIResult();
        Check(TEXT("reset during landing does not claim completed landing"),R.bSaved&&R.StopReason==TEXT("manual_stop")&&R.CompletionReason==TEXT("reset")&&R.SessionId!=FirstId);
        Finish();return;
    }
    if(Stage==9&&Age>1){Finish();}
    if(Stage==10&&Age>1){Check(TEXT("active practice exits without pretending it landed"),M->IsAIPracticeActive());Finish();}
}
