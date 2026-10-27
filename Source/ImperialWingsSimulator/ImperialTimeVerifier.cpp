#include "ImperialTimeVerifier.h"
#include "ImperialMatchManager.h"
#include "ImperialBuildInfo.h"
#include "SoccerDronePawn.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "UnrealClient.h"
#include "Physics/Experimental/PhysScene_Chaos.h"
#include "PhysicsEngine/PhysicsSettings.h"
#include "PBDRigidsSolver.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/FileHelper.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "HAL/PlatformMisc.h"

AImperialTimeVerifier::AImperialTimeVerifier()
{PrimaryActorTick.bCanEverTick=true;PrimaryActorTick.TickGroup=TG_PostPhysics;}
void AImperialTimeVerifier::BeginPlay()
{
    Super::BeginPlay();FApp::SetUseFixedTimeStep(false);GEngine->SetMaxFPS(60);
    bHitches=FParse::Param(FCommandLine::Get(),TEXT("ImperialTimeHitches"));
    bOfficial=FParse::Param(FCommandLine::Get(),TEXT("ImperialTimeOfficial"));
}
void AImperialTimeVerifier::Tick(float Dt)
{
    Super::Tick(Dt);Age+=Dt;
    auto* M=Cast<AImperialMatchManager>(UGameplayStatics::GetActorOfClass(GetWorld(),AImperialMatchManager::StaticClass()));
    auto* Scene=GetWorld()->GetPhysicsScene();
    if(!M||M->GetDrones().Num()!=10||!Scene||!Scene->GetSolver())return;
    const double SolverNow=Scene->GetSolver()->GetSolverTime();
    if(!bLaunched&&Age>1)
    {
        if(bOfficial)M->StartMatch();
        for(int I=0;I<10;++I){auto* D=M->GetDrone(I);Targets[I]=D->GetActorLocation();if(!bOfficial){Targets[I].Z=120;D->SetAutonomousEnabled(true);D->SetAutonomousTarget(Targets[I]);}}
        bLaunched=true;
    }
    if(!bMeasuring)
    {
        if(Age<(bOfficial?12:8))return;bMeasuring=true;GameStart=M->GetElapsed();SolverStart=LastSolver=SolverNow;WallStart=FPlatformTime::Seconds();
        LedgerGameStart=M->GetSimulationTime().GameSeconds;LedgerPhysicsStart=M->GetSimulationTime().PhysicsSeconds;LedgerDroppedStart=M->GetSimulationTime().DroppedSeconds;
        M->ToggleFlightRecording();
        for(int I=0;I<10;++I){auto* D=M->GetDrone(I);ModelStart[I]=D->GetFlightModelSeconds();SensorStart[I]=D->GetSensorModelSeconds();BatteryPrevious[I]=D->GetBatteryConsumedAh();}
        return;
    }
    GameTotal+=Dt;const double PhysicsDt=SolverNow-LastSolver;LastSolver=SolverNow;PhysicsTotal+=PhysicsDt;
    auto Row=MakeShared<FJsonObject>();Row->SetNumberField(TEXT("game_dt_s"),Dt);Row->SetNumberField(TEXT("solver_dt_s"),PhysicsDt);Row->SetNumberField(TEXT("at_s"),GameTotal);
    double FrameModelError=0,FrameSensorError=0;
    for(int I=0;I<10;++I)
    {
        auto* D=M->GetDrone(I);FrameModelError=FMath::Max(FrameModelError,FMath::Abs(D->GetFlightModelSeconds()-ModelStart[I]-PhysicsTotal));
        if(bOfficial)bMatchSafe&=!D->IsArmed()&&!D->IsAutonomousEnabled();
        FrameSensorError=FMath::Max(FrameSensorError,FMath::Abs(D->GetSensorModelSeconds()-SensorStart[I]-PhysicsTotal));
        const double Consumed=D->GetBatteryConsumedAh();BatteryError[I]+=Consumed-BatteryPrevious[I]-D->GetBatteryCurrentA()*PhysicsDt/3600.;BatteryPrevious[I]=Consumed;
        MaximumTilt=FMath::Max(MaximumTilt,(double)D->GetTiltDegrees());MaximumSpeed=FMath::Max(MaximumSpeed,(double)D->GetFlightSpeed());
        MaximumHoldError=FMath::Max(MaximumHoldError,(double)FVector::Distance(D->GetActorLocation(),Targets[I]));
    }
    LargestModelError=FMath::Max(LargestModelError,FrameModelError);LargestSensorError=FMath::Max(LargestSensorError,FrameSensorError);
    Row->SetNumberField(TEXT("max_model_accum_error_s"),FrameModelError);Row->SetNumberField(TEXT("max_sensor_accum_error_s"),FrameSensorError);Frames.Add(MakeShared<FJsonValueObject>(Row));
    if(bHitches&&!bInjected100&&GameTotal>10){bInjected100=true;FPlatformProcess::Sleep(.1f);}
    if(bHitches&&!bInjected250&&GameTotal>30){bInjected250=true;FPlatformProcess::Sleep(.25f);}
    FString Capture;FParse::Value(FCommandLine::Get(),TEXT("ImperialTimeCapture="),Capture);
    if(!Capture.IsEmpty()&&!bCaptureRequested&&GameTotal>31){bCaptureRequested=true;FScreenshotRequest::RequestScreenshot(Capture,true,false);}
    if(GameTotal<60)return;
    M->ToggleFlightRecording();
    double MaxBatteryError=0;for(double E:BatteryError)MaxBatteryError=FMath::Max(MaxBatteryError,FMath::Abs(E));
    const auto& Ledger=M->GetSimulationTime();
    const double LedgerGame=Ledger.GameSeconds-LedgerGameStart,LedgerPhysics=Ledger.PhysicsSeconds-LedgerPhysicsStart,LedgerDropped=Ledger.DroppedSeconds-LedgerDroppedStart;
    const double MatchTime=bOfficial?GameStart-M->GetElapsed():M->GetElapsed()-GameStart;
    const bool bClockOk=LargestModelError<.008334&&LargestSensorError<.008334&&MaxBatteryError<1e-7&&
        FMath::Abs(LedgerPhysics-PhysicsTotal)<1e-5&&FMath::Abs(LedgerGame-GameTotal)<1e-5&&FMath::Abs(LedgerDropped-(GameTotal-PhysicsTotal))<1e-5&&FMath::Abs(MatchTime-GameTotal)<1e-5;
    const bool bStable=FMath::IsFinite(MaximumHoldError)&&MaximumHoldError<60&&MaximumTilt<35&&MaximumSpeed<2;
    const bool bSync=!UPhysicsSettings::Get()->bTickPhysicsAsync;
    auto Result=MakeShared<FJsonObject>();Result->SetBoolField(TEXT("ok"),bClockOk&&bStable&&bSync&&bMatchSafe);
    Result->SetBoolField(TEXT("injected_100ms"),bInjected100);Result->SetBoolField(TEXT("injected_250ms"),bInjected250);
    Result->SetBoolField(TEXT("official_manual_match"),bOfficial);Result->SetBoolField(TEXT("match_stayed_manual_disarmed"),bMatchSafe);
    Result->SetObjectField(TEXT("ledger"),Ledger.Json());Result->SetNumberField(TEXT("measured_dropped_s"),LedgerDropped);
    Result->SetBoolField(TEXT("clocks_ok"),bClockOk);Result->SetBoolField(TEXT("stable"),bStable);Result->SetBoolField(TEXT("synchronous_physics"),bSync);
    Result->SetObjectField(TEXT("build"),ImperialBuildInfo::Json());Result->SetBoolField(TEXT("hitches"),bHitches);
    Result->SetNumberField(TEXT("game_s"),GameTotal);Result->SetNumberField(TEXT("match_clock_elapsed_s"),MatchTime);
    Result->SetNumberField(TEXT("solver_s"),SolverNow-SolverStart);Result->SetNumberField(TEXT("wall_s"),FPlatformTime::Seconds()-WallStart);
    Result->SetNumberField(TEXT("max_model_clock_error_s"),LargestModelError);Result->SetNumberField(TEXT("max_sensor_clock_error_s"),LargestSensorError);
    Result->SetNumberField(TEXT("max_battery_integral_error_ah"),MaxBatteryError);Result->SetNumberField(TEXT("max_hold_error_cm"),MaximumHoldError);
    Result->SetNumberField(TEXT("max_tilt_deg"),MaximumTilt);Result->SetNumberField(TEXT("max_speed_mps"),MaximumSpeed);Result->SetArrayField(TEXT("frames"),Frames);
    Result->SetStringField(TEXT("scope"),TEXT("60 real game seconds; ten real Chaos bodies; local autonomous HOLD; optional actual 100/250ms process sleeps; no USB or APF server; not a fixed-thread PID"));
    FString Text,File;FJsonSerializer::Serialize(Result,TJsonWriterFactory<>::Create(&Text));FParse::Value(FCommandLine::Get(),TEXT("ImperialTimeResult="),File);FFileHelper::SaveStringToFile(Text,*File);
    FPlatformMisc::RequestExitWithStatus(false,bClockOk&&bStable&&bSync&&bMatchSafe?0:1);SetActorTickEnabled(false);
}
