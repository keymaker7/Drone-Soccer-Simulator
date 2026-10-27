#include "ImperialPowerVerifier.h"
#include "ImperialMatchManager.h"
#include "ImperialBuildInfo.h"
#include "SoccerDronePawn.h"
#include "Kismet/GameplayStatics.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/GameViewportClient.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformMisc.h"

AImperialPowerVerifier::AImperialPowerVerifier()
{PrimaryActorTick.bCanEverTick=true;PrimaryActorTick.TickGroup=TG_PostPhysics;}
void AImperialPowerVerifier::Tick(float Dt)
{
    Super::Tick(Dt);Age+=Dt;if(Scenario>=0){MinDt=FMath::Min(MinDt,(double)Dt);MaxDt=FMath::Max(MaxDt,(double)Dt);}
    auto* M=Cast<AImperialMatchManager>(UGameplayStatics::GetActorOfClass(GetWorld(),AImperialMatchManager::StaticClass()));
    auto* D=M?M->GetSelectedDrone():nullptr;if(!D)return;
    const double Limits[]={80,10,0,.1,80,80};
    const double Charge[]={1,1,1,1,0,1};
    const float Throttles[]={1,1,1,1,1,.4f};
    const TCHAR* Names[]={TEXT("full_throttle_80A"),TEXT("full_throttle_10A"),TEXT("zero_current_supply"),TEXT("avionics_brownout"),TEXT("empty_battery"),TEXT("moderate_throttle")};
    const auto Begin=[&](int32 Next)
    {
        Scenario=Next;Age=0;Samples=0;MaxPowerExcess=MaxAvailableExcess=MaxCurrent=MaxThrust=MaxAltitude=0;bLimited=false;
        D->SetAutonomousEnabled(false);D->ResetDrone();ModelStart=D->GetFlightModelSeconds();
        D->SetActorLocation(FVector(-350,-150,100),false,nullptr,ETeleportType::TeleportPhysics);
        D->GetBodyMesh()->SetPhysicsLinearVelocity(FVector::ZeroVector);D->GetBodyMesh()->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector);
        D->SetPowerVerificationConditions(Limits[Next],Charge[Next]);D->SetAngleVerificationInput(FVector::ZeroVector,0);bOk&=D->SetArmed(true);
        D->SetAngleVerificationInput(FVector::ZeroVector,Throttles[Next]);
    };
    if(Scenario<0){if(Age>.6){M->ResetMatch();M->ToggleFlightRecording();Begin(0);}return;}
    ++Samples;
    // Independent energy bound: motors cannot draw more than all terminal power,
    // even before reserving any avionics. Inspect the thrust actually sent to Chaos.
    MaxPowerExcess=FMath::Max(MaxPowerExcess,D->GetMotorElectricalPowerW()-D->GetBatteryDeliveredPowerW());
    MaxAvailableExcess=FMath::Max(MaxAvailableExcess,D->GetMotorElectricalPowerW()-D->GetAvailableMotorPowerW());
    MaxCurrent=FMath::Max(MaxCurrent,D->GetBatteryCurrentA());MaxThrust=FMath::Max(MaxThrust,(double)D->GetTotalThrustNewtons());
    MaxAltitude=FMath::Max(MaxAltitude,(double)D->GetAltitudeMeters());bLimited|=D->IsBatteryPowerLimited();
    FString Capture;FParse::Value(FCommandLine::Get(),TEXT("ImperialPowerCapture="),Capture);
    if(Scenario==1&&Age>.35&&!bCaptured&&!Capture.IsEmpty()){bCaptured=true;FScreenshotRequest::RequestScreenshot(Capture,true,false);}
    if(D->GetFlightModelSeconds()-ModelStart<.8)return;
    const bool DeadSupply=Scenario==2||Scenario==3||Scenario==4;
    const bool Passed=Samples>4&&FMath::IsFinite(MaxPowerExcess)&&MaxPowerExcess<1e-4&&MaxAvailableExcess<1e-4&&MaxCurrent<=Limits[Scenario]+1e-6&&
        (!DeadSupply||(MaxThrust<1e-6&&MaxAltitude<1.02))&&((Scenario>=1&&Scenario<=3)?bLimited:true)&&
        (DeadSupply||MaxThrust>1.)&&(Scenario!=0||(MaxThrust>9.&&MaxAltitude>1.1));
    auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("scenario"),Names[Scenario]);Row->SetBoolField(TEXT("ok"),Passed);
    Row->SetNumberField(TEXT("samples"),Samples);Row->SetNumberField(TEXT("maximum_current_a"),MaxCurrent);
    Row->SetNumberField(TEXT("model_seconds"),D->GetFlightModelSeconds()-ModelStart);
    Row->SetNumberField(TEXT("maximum_motor_power_excess_w"),MaxPowerExcess);Row->SetNumberField(TEXT("maximum_thrust_n"),MaxThrust);
    Row->SetNumberField(TEXT("maximum_available_power_excess_w"),MaxAvailableExcess);
    Row->SetNumberField(TEXT("maximum_altitude_m"),MaxAltitude);Row->SetBoolField(TEXT("power_limited_observed"),bLimited);
    Results.Add(MakeShared<FJsonValueObject>(Row));bOk&=Passed;
    if(Scenario<5){Begin(Scenario+1);return;}
    D->SetArmed(false);M->ToggleFlightRecording();
    auto Result=MakeShared<FJsonObject>();Result->SetBoolField(TEXT("ok"),bOk);Result->SetArrayField(TEXT("scenarios"),Results);Result->SetObjectField(TEXT("build"),ImperialBuildInfo::Json());
    Result->SetNumberField(TEXT("actual_min_game_dt_s"),MinDt);Result->SetNumberField(TEXT("actual_max_game_dt_s"),MaxDt);
    Result->SetStringField(TEXT("scope"),TEXT("explicit synthetic non-USB throttle fixture; actual Chaos body/force path; experimental electrical model, not measured aircraft power"));
    FString Json,File;FJsonSerializer::Serialize(Result,TJsonWriterFactory<>::Create(&Json));FParse::Value(FCommandLine::Get(),TEXT("ImperialPowerResult="),File);
    FFileHelper::SaveStringToFile(Json,*File,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    FPlatformMisc::RequestExitWithStatus(false,bOk?0:1);SetActorTickEnabled(false);
}
