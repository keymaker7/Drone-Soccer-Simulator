#include "ImperialAIRecoveryVerifier.h"
#include "ImperialMatchManager.h"
#include "ImperialPlayerController.h"
#include "ImperialHUD.h"
#include "ImperialBuildInfo.h"
#include "SoccerDronePawn.h"
#include "Components/StaticMeshComponent.h"
#include "PhysicsEngine/PhysicsConstraintComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/FileHelper.h"
#include "Engine/Engine.h"
#include "UnrealClient.h"
#include "HAL/PlatformMisc.h"
AImperialAIRecoveryVerifier::AImperialAIRecoveryVerifier(){PrimaryActorTick.bCanEverTick=true;PrimaryActorTick.TickGroup=TG_PostPhysics;}
void AImperialAIRecoveryVerifier::Check(const TCHAR* Name,bool Passed,double Value)
{auto R=MakeShared<FJsonObject>();R->SetNumberField(TEXT("case"),Case);R->SetStringField(TEXT("check"),Name);R->SetBoolField(TEXT("ok"),Passed);R->SetNumberField(TEXT("value"),Value);Checks.Add(MakeShared<FJsonValueObject>(R));bOk&=Passed;}
void AImperialAIRecoveryVerifier::ReleaseObstruction()
{if(Obstruction){Obstruction->BreakConstraint();Obstruction->DestroyComponent();Obstruction=nullptr;}}
void AImperialAIRecoveryVerifier::AttachObstruction(ASoccerDronePawn* D)
{
    ReleaseObstruction();Obstruction=NewObject<UPhysicsConstraintComponent>(this);Obstruction->RegisterComponent();Obstruction->SetWorldLocation(D->GetActorLocation());
    Obstruction->SetLinearXLimit(ELinearConstraintMotion::LCM_Locked,0);Obstruction->SetLinearYLimit(ELinearConstraintMotion::LCM_Locked,0);Obstruction->SetLinearZLimit(ELinearConstraintMotion::LCM_Locked,0);
    Obstruction->SetAngularSwing1Limit(EAngularConstraintMotion::ACM_Free,0);Obstruction->SetAngularSwing2Limit(EAngularConstraintMotion::ACM_Free,0);Obstruction->SetAngularTwistLimit(EAngularConstraintMotion::ACM_Free,0);
    Obstruction->SetConstrainedComponents(D->GetBodyMesh(),NAME_None,nullptr,NAME_None);
}
void AImperialAIRecoveryVerifier::Finish()
{
    ReleaseObstruction();auto R=MakeShared<FJsonObject>();R->SetBoolField(TEXT("ok"),bOk);R->SetArrayField(TEXT("checks"),Checks);R->SetObjectField(TEXT("build"),ImperialBuildInfo::Json());
    R->SetNumberField(TEXT("min_game_dt_s"),MinDt);R->SetNumberField(TEXT("max_game_dt_s"),MaxDt);
    R->SetStringField(TEXT("scope"),TEXT("Actual body collision; constrained obstruction/release; explicit inverted initial fixture; motor disable. Not calibrated net entanglement or physical USB flight."));
    FString File,Json;FParse::Value(FCommandLine::Get(),TEXT("ImperialAIRecoveryResult="),File);FJsonSerializer::Serialize(R,TJsonWriterFactory<>::Create(&Json));
    const bool Saved=FFileHelper::SaveStringToFile(Json,*File,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);SetActorTickEnabled(false);FPlatformMisc::RequestExitWithStatus(false,bOk&&Saved?0:1);
}
void AImperialAIRecoveryVerifier::Tick(float Dt)
{
    Super::Tick(Dt);Age+=Dt;Total+=Dt;MinDt=FMath::Min(MinDt,static_cast<double>(Dt));MaxDt=FMath::Max(MaxDt,static_cast<double>(Dt));
    if(Total>220){Check(TEXT("bounded verification"),false);Finish();return;}
    auto* M=Cast<AImperialMatchManager>(UGameplayStatics::GetActorOfClass(GetWorld(),AImperialMatchManager::StaticClass()));auto* PC=Cast<AImperialPlayerController>(UGameplayStatics::GetPlayerController(this,0));auto* H=PC?Cast<AImperialHUD>(PC->GetHUD()):nullptr;
    if(!M||!H||Age<.25)return;auto* D=M->GetDrone(6);auto* Player=M->GetDrone(0);
    if(Stage==0)
    {
        if(FParse::Param(FCommandLine::Get(),TEXT("ImperialAIRecoveryReload")))
        {
            FString Reason;FParse::Value(FCommandLine::Get(),TEXT("ImperialAIRecoveryExpected="),Reason);const auto& R=M->GetAIResult();
            Check(TEXT("persisted recovery result restores reason and attempts"),R.Exists()&&R.bSaved&&R.StopReason==Reason&&R.RecoveryAttempts==(Reason==TEXT("recovery_failed")?2:0));
            Check(TEXT("loading a fault result cannot launch any aircraft"),!M->IsAIPracticeActive()&&!Player->IsArmed());Finish();return;
        }
        FParse::Value(FCommandLine::Get(),TEXT("ImperialAIRecoveryCapture="),Capture);if(GEngine)GEngine->SetMaxFPS(Capture.IsEmpty()?0:60);
        ReleaseObstruction();M->ResetMatch();if(H->IsPilotMenuOpen()){if(H->IsRadioPanelOpen())H->ToggleRadioPanel();else H->ToggleAIPracticePanel();}
        PreviousResultId=M->GetAIResult().SessionId;
        Check(TEXT("practice starts with no player auto-arm"),M->StartAIPractice({3,0,1})&&!Player->IsArmed());Stage=1;Age=0;bFinite=bPlayerSafe=true;return;
    }
    for(auto Body:M->GetDrones())bFinite&=!Body->GetActorLocation().ContainsNaN()&&Body->GetFlightSpeed()<12;
    bPlayerSafe&=!Player->IsArmed()&&!Player->IsAutonomousEnabled();
    if(Stage==1&&Age>4)
    {
        if(Case==1||Case==2){AttachObstruction(D);Stage=3;Age=0;return;}
        if(Age<10)return;
        if(Case==0)
        {
            InitialPosition=D->GetActorLocation();InitialHits=D->GetGuardImpactCount();PeakDisplacement=0;
            // One explicit initial placement of the impactor, then Chaos collision.
            Player->ResetDrone();Player->GetBodyMesh()->SetWorldLocationAndRotation(InitialPosition+FVector(-90,0,0),FRotator::ZeroRotator,false,nullptr,ETeleportType::TeleportPhysics);
            Player->GetBodyMesh()->SetPhysicsLinearVelocity(FVector(500,0,0));Stage=2;Age=0;return;
        }
        if(Case==3)
        {
            // Deterministic post-crash initial condition, never a recovery teleport.
            D->GetBodyMesh()->SetWorldLocationAndRotation(FVector(550,0,22),FRotator(180,0,0),false,nullptr,ETeleportType::TeleportPhysics);
            D->GetBodyMesh()->SetPhysicsLinearVelocity(FVector::ZeroVector);D->GetBodyMesh()->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector);
        }
        if(Case==4)for(int32 I=6;I<9;++I)M->GetDrone(I)->SetAutonomousEnabled(false);
        Stage=5;Age=0;return;
    }
    if(Stage==2)
    {
        PeakDisplacement=FMath::Max(PeakDisplacement,FVector::Distance(D->GetActorLocation(),InitialPosition)/100.);
        if(Age<10)return;
        Check(TEXT("two real bodies register an impact and displacement"),D->GetGuardImpactCount()>InitialHits&&Player->GetGuardImpactCount()>0&&PeakDisplacement>.1,PeakDisplacement);
        Check(TEXT("normal contact recovers through motor control"),M->IsAIPracticeActive()&&!M->IsAILanding()&&D->GetAltitudeMeters()>.7&&D->GetTiltDegrees()<30&&FVector::Distance(D->GetActorLocation(),D->GetAutonomousTarget())<90);
        M->StopAIPractice();Stage=6;Age=0;return;
    }
    if(Stage==3)
    {
        if(Case==1&&M->GetAIRecoveringCount()>0)
        {Check(TEXT("blocked actual body triggers bounded escape"),M->GetAIRecoveryAttempts()==1&&D->IsArmed());ReleaseObstruction();Stage=4;Age=0;return;}
        if(Case==2&&M->IsAILanding())
        {Check(TEXT("persistent obstruction stops after two attempts"),M->GetAIRecoveryAttempts()==2);ReleaseObstruction();Stage=6;Age=0;return;}
        if(Age>22){Check(TEXT("obstruction detection completes within bound"),false);ReleaseObstruction();M->StopAIPractice();Stage=6;Age=0;}return;
    }
    if(Stage==4&&Age>10)
    {
        Check(TEXT("released obstruction recovers without teleport"),M->IsAIPracticeActive()&&!M->IsAILanding()&&M->GetAIRecoveringCount()==0&&D->GetAltitudeMeters()>.7&&FVector::Distance(D->GetActorLocation(),D->GetAutonomousTarget())<90);
        M->StopAIPractice();Stage=6;Age=0;return;
    }
    if(Stage==5)
    {
        if(M->IsAILanding())
        {
            Check(TEXT("failed defender initiates team landing without player auto-arm"),bPlayerSafe);
            if(Case==3)Check(TEXT("inverted grounded defender motors stop"),!D->IsArmed()&&!D->IsAutonomousEnabled()&&D->GetAltitudeMeters()<.34);
            if(Case==4)Check(TEXT("disabled airborne bodies are not reported as landed"),D->GetAltitudeMeters()>.35&&!D->IsArmed()&&M->GetAIResult().SessionId==PreviousResultId);
            Stage=6;Age=0;return;
        }
        if(Age>3){Check(TEXT("failed defender detected promptly"),false);M->StopAIPractice();Stage=6;Age=0;}return;
    }
    if(Stage==6&&(!M->IsAIPracticeActive()||Age>30))
    {
        bool Landed=!M->IsAIPracticeActive();for(int32 I=6;I<9;++I)Landed&=!M->GetDrone(I)->IsArmed()&&M->GetDrone(I)->GetAltitudeMeters()<.32;
        Check(TEXT("all defenders physically land and disarm"),Landed);Check(TEXT("finite bodies and manual player ownership retained"),bFinite&&bPlayerSafe);
        const auto& R=M->GetAIResult();const FString Reason=Case==2?TEXT("recovery_failed"):(Case==3?TEXT("defender_grounded"):(Case==4?TEXT("defender_unavailable"):TEXT("manual_stop")));
        Check(TEXT("result records the real stop reason and attempt count"),R.bSaved&&R.bTestSession&&R.StopReason==Reason&&R.CompletionReason==TEXT("landed")&&R.RecoveryAttempts==M->GetAIRecoveryAttempts());
        if(!Capture.IsEmpty()&&Case==2){H->ToggleAIPracticePanel();H->NotifyHitBoxClick(TEXT("AI_Last"));FScreenshotRequest::RequestScreenshot(Capture,true,false);Stage=9;Age=0;return;}
        if(++Case==5){Finish();return;}Stage=0;Age=0;return;
    }
    if(Stage==9&&Age>1)Finish();
}
