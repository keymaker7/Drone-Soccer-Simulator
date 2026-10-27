#include "ImperialThrottleVerifier.h"
#include "ImperialRadioSubsystem.h"
#include "ImperialMatchManager.h"
#include "ImperialPlayerController.h"
#include "ImperialHUD.h"
#include "ImperialBuildInfo.h"
#include "SoccerDronePawn.h"
#include "Kismet/GameplayStatics.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/GameInstance.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProcess.h"

AImperialThrottleVerifier::AImperialThrottleVerifier()
{PrimaryActorTick.bCanEverTick=true;PrimaryActorTick.TickGroup=TG_PostPhysics;}
void AImperialThrottleVerifier::Check(const TCHAR* Name,bool Passed,double Value)
{
    auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("check"),Name);Row->SetBoolField(TEXT("ok"),Passed);Row->SetNumberField(TEXT("value"),Value);
    Checks.Add(MakeShared<FJsonValueObject>(Row));bOk&=Passed;
}
void AImperialThrottleVerifier::Finish()
{
    auto Result=MakeShared<FJsonObject>();Result->SetBoolField(TEXT("ok"),bOk);Result->SetArrayField(TEXT("checks"),Checks);
    Result->SetBoolField(TEXT("reloaded"),FParse::Param(FCommandLine::Get(),TEXT("ImperialThrottleReload")));
    Result->SetObjectField(TEXT("build"),ImperialBuildInfo::Json());
    Result->SetNumberField(TEXT("actual_min_game_dt_s"),MinDt);Result->SetNumberField(TEXT("actual_max_game_dt_s"),MaxDt);
    Result->SetStringField(TEXT("scope"),TEXT("isolated saved control preference and explicit non-USB raw throttle fixture through actual Chaos motor forces; no altitude hold or physical-controller calibration"));
    FString Json,File;FJsonSerializer::Serialize(Result,TJsonWriterFactory<>::Create(&Json));FParse::Value(FCommandLine::Get(),TEXT("ImperialThrottleResult="),File);
    FFileHelper::SaveStringToFile(Json,*File,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    FPlatformMisc::RequestExitWithStatus(false,bOk?0:1);SetActorTickEnabled(false);
}
void AImperialThrottleVerifier::Tick(float Dt)
{
    Super::Tick(Dt);Age+=Dt;if(Age>.1){MinDt=FMath::Min(MinDt,(double)Dt);MaxDt=FMath::Max(MaxDt,(double)Dt);}
    // Fixed game time otherwise runs faster than wall time under NullRHI.
    // Pace only this explicit fixture so the ordinary 50 ms F6 recorder can
    // observe every short full-power segment without forced synthetic rows.
    FPlatformProcess::Sleep(FMath::Clamp(Dt,0.f,.05f));
    auto* M=Cast<AImperialMatchManager>(UGameplayStatics::GetActorOfClass(GetWorld(),AImperialMatchManager::StaticClass()));
    auto* PC=Cast<AImperialPlayerController>(UGameplayStatics::GetPlayerController(this,0));
    auto* H=PC?Cast<AImperialHUD>(PC->GetHUD()):nullptr;
    auto* R=GetGameInstance()->GetSubsystem<UImperialRadioSubsystem>();auto* D=M?M->GetSelectedDrone():nullptr;
    if(!H||!R||!D)return;
    const bool Reload=FParse::Param(FCommandLine::Get(),TEXT("ImperialThrottleReload"));
    const float Inputs[]={.54f,.56f,1.f,.54f,.56f,1.f};
    const auto Begin=[&](int32 Next)
    {
        Scenario=Next;Age=0;D->SetAutonomousEnabled(false);D->ResetDrone();ModelStart=D->GetFlightModelSeconds();
        Check(TEXT("curve change succeeds only while disarmed"),R->SetPrecisionThrottleEnabled(Next>=3));
        D->SetActorLocation(FVector(-350,-150,100),false,nullptr,ETeleportType::TeleportPhysics);
        D->GetBodyMesh()->SetPhysicsLinearVelocity(FVector::ZeroVector);D->GetBodyMesh()->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector);
        D->SetAngleVerificationInput(FVector::ZeroVector,0);Check(TEXT("explicit zero raw throttle ARM"),D->SetArmed(true));
        D->SetAngleVerificationInput(FVector::ZeroVector,Inputs[Next]);
    };
    if(Scenario<0)
    {
        if(Age<.6)return;
        M->ResetMatch();Check(TEXT("cold launch remains keyboard and disarmed"),!R->IsEnabled()&&!D->IsArmed());
        Check(Reload?TEXT("precision restored in a second process"):TEXT("new user defaults to direct throttle"),R->IsPrecisionThrottleEnabled()==Reload);
        if(!Reload)
        {
            if(!H->IsRadioPanelOpen())H->ToggleRadioPanel();H->NotifyHitBoxClick(TEXT("Radio_PrecisionThrottle"));
            Check(TEXT("HUD precision button saves enabled setting"),R->IsPrecisionThrottleEnabled());
            Check(TEXT("preference does not create USB calibration"),!IFileManager::Get().FileExists(*R->GetProfilePath()));
            Check(TEXT("precision keeps zero and full endpoints"),R->ApplyManualThrottle(0)==0&&R->ApplyManualThrottle(1)==1);
            H->ToggleRadioPanel();D->SetAngleVerificationInput(FVector::ZeroVector,0);D->SetArmed(true);
            Check(TEXT("armed manual aircraft blocks curve change"),!R->SetPrecisionThrottleEnabled(false)&&R->IsPrecisionThrottleEnabled());
            D->SetArmed(false);Finish();return;
        }
        if(H->IsRadioPanelOpen())H->ToggleRadioPanel();M->ToggleFlightRecording();Begin(0);return;
    }
    // Measure settled full power before the aircraft reaches the ceiling; use
    // integrated model time rather than render/game time after a long frame.
    if(D->GetFlightModelSeconds()-ModelStart<(Scenario==2||Scenario==5?.25:.9))return;
    FinalThrust[Scenario]=D->GetTotalThrustNewtons();
    Check(TEXT("raw throttle remains unmodified"),FMath::IsNearlyEqual(D->GetThrottle(),Inputs[Scenario],1e-6f));
    Check(TEXT("actual force command follows selected curve"),FMath::IsNearlyEqual(D->GetThrottleCommand(),R->ApplyManualThrottle(Inputs[Scenario]),1e-6f));
    Check(TEXT("motor force remains within battery power"),D->GetMotorElectricalPowerW()<=D->GetBatteryDeliveredPowerW()+.001&&D->GetMotorElectricalPowerW()<=D->GetAvailableMotorPowerW()+.001);
    if(Scenario<5){Begin(Scenario+1);return;}
    D->SetArmed(false);M->ToggleFlightRecording();
    const double DirectDelta=FinalThrust[1]-FinalThrust[0],PrecisionDelta=FinalThrust[4]-FinalThrust[3];
    const double Ratio=DirectDelta>1e-6?PrecisionDelta/DirectDelta:0;
    Check(TEXT("precision reduces actual thrust change around the chosen middle"),Ratio>.55&&Ratio<.8,Ratio);
    Check(TEXT("precision preserves actual full-throttle output"),FMath::Abs(FinalThrust[5]-FinalThrust[2])<.01,FMath::Abs(FinalThrust[5]-FinalThrust[2]));
    D->SetAngleVerificationInput(FVector::ZeroVector,.1f);Check(TEXT("high raw throttle still blocks ARM"),!D->SetArmed(true));
    Check(TEXT("native radio never enabled by curve preference"),!R->IsEnabled());
    Finish();
}
