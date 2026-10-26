#include "ImperialMatchManager.h"
#include "SoccerDronePawn.h"
#include "ImperialPlayerController.h"
#include "ImperialRadioSubsystem.h"
#include "PrototypeArenaBuilder.h"
#include "ImperialElasticNet.h"
#include "ImperialSoccerLayout.h"
#include "ImperialBuildInfo.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "UnrealClient.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "HttpModule.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "HAL/PlatformMisc.h"

namespace
{
TArray<TSharedPtr<FJsonValue>> JsonVector(FVector V)
{
    return {MakeShared<FJsonValueNumber>(V.X),MakeShared<FJsonValueNumber>(V.Y),MakeShared<FJsonValueNumber>(V.Z)};
}
bool ReadVector(const TSharedPtr<FJsonObject>& O,const TCHAR* Key,FVector& V)
{
    const TArray<TSharedPtr<FJsonValue>>* A=nullptr;
    if(!O.IsValid()||!O->TryGetArrayField(Key,A)||!A||A->Num()!=3) return false;
    for(const auto& Item:*A)if(!Item.IsValid())return false;
    double X,Y,Z;if(!(*A)[0]->TryGetNumber(X)||!(*A)[1]->TryGetNumber(Y)||!(*A)[2]->TryGetNumber(Z))return false;
    V=FVector(X,Y,Z);return !V.ContainsNaN()&&FMath::IsFinite(X)&&FMath::IsFinite(Y)&&FMath::IsFinite(Z);
}
void AttachVerificationIdentity(FString& Value)
{
    TSharedPtr<FJsonObject> Root;
    if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Value),Root)||!Root.IsValid())return;
    Root->SetObjectField(TEXT("build"),ImperialBuildInfo::Json());
    FString Settings;TSharedPtr<FJsonObject> Defaults;
    if(FFileHelper::LoadFileToString(Settings,*FPaths::Combine(FPaths::ProjectContentDir(),TEXT("Data/physics-defaults.json")))&&
       FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Settings),Defaults)&&Defaults.IsValid())Root->SetObjectField(TEXT("physics_defaults"),Defaults);
    Value.Reset();FJsonSerializer::Serialize(Root.ToSharedRef(),TJsonWriterFactory<>::Create(&Value));
}
}
AImperialMatchManager::AImperialMatchManager(){PrimaryActorTick.bCanEverTick=true;}
void AImperialMatchManager::BeginPlay()
{
    Super::BeginPlay();
    ImperialBuildInfo::WriteRuntimeIdentity();
    GuidanceSession=FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);
    FString RequestedUrl;
    if(FParse::Value(FCommandLine::Get(),TEXT("ImperialBridgeUrl="),RequestedUrl)&&RequestedUrl.StartsWith(TEXT("http://127.0.0.1:")))GuidanceUrl=RequestedUrl;
    GuidanceUrl.RemoveFromEnd(TEXT("/"));
    bSmoke=FParse::Param(FCommandLine::Get(),TEXT("ImperialPhysicsSmoke"));
    bAngleSmoke=FParse::Param(FCommandLine::Get(),TEXT("ImperialAngleSmoke"));
    bDemo=FParse::Param(FCommandLine::Get(),TEXT("ImperialDemo"));
    FParse::Value(FCommandLine::Get(),TEXT("ImperialCapture="),CaptureFile);
    FParse::Value(FCommandLine::Get(),TEXT("ImperialCaptureTime="),CaptureAt);
    FParse::Value(FCommandLine::Get(),TEXT("ImperialResult="),SmokeResultFile);
    int32 RequestedView=1;FParse::Value(FCommandLine::Get(),TEXT("ImperialView="),RequestedView);CameraView=FMath::Clamp(RequestedView,1,3);
    if(FParse::Param(FCommandLine::Get(),TEXT("ImperialShowcase")))CameraView=3;
    if(GEngine&&!bSmoke&&!bAngleSmoke)GEngine->SetMaxFPS(120);
    for(int32 I=0;I<10;++I)
    {
        const FVector Home=ImperialSoccerLayout::StartPosition(I<5?0:1,I%5);
        FActorSpawnParameters Params; Params.Name=FName(*FString::Printf(TEXT("IW_%02d"),I+1));Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        auto* D=GetWorld()->SpawnActor<ASoccerDronePawn>(ASoccerDronePawn::StaticClass(),Home,FRotator(0,I<5?0:180,0),Params);
        D->SetTeamColor(GetTeamColor(I));D->SetPlayerRole(I%5); Drones.Add(D);Homes.Add(Home);PreviousPositions.Add(Home);GoalReady.Add(true);
    }
    Log(TEXT("10 aircraft ready. Keyboard input selected; USB axes are uncalibrated."));
    UE_LOG(LogTemp,Display,TEXT("IMPERIAL_RUNTIME_READY drones=%d physics=Chaos backend=SDACS_APF"),Drones.Num());
}
void AImperialMatchManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    AdvanceGuidanceCommand();
    Super::EndPlay(EndPlayReason);
}
void AImperialMatchManager::CancelPendingGuidance()
{
    auto Previous=PendingGuidance;
    PendingGuidance.Reset();bRequestPending=false;GuidanceWindow.Abort();
    if(Previous.IsValid()){Previous->OnProcessRequestComplete().Unbind();Previous->CancelRequest();}
}
void AImperialMatchManager::AdvanceGuidanceCommand()
{
    GuidanceWindow.NewCommand();CancelPendingGuidance();LastRequestWallTime=0;
}
FString AImperialMatchManager::GuidanceMode() const
{
    return Mode==EImperialMode::Mission?TEXT("mission"):(Mode==EImperialMode::Returning?TEXT("return"):TEXT("hold"));
}
FLinearColor AImperialMatchManager::GetTeamColor(int32 Id)const{return Id<5?FLinearColor(.02f,.22f,1.f):FLinearColor(1.f,.025f,.035f);}
ASoccerDronePawn* AImperialMatchManager::GetDrone(int32 Id)const{return Drones.IsValidIndex(Id)?Drones[Id].Get():nullptr;}
ASoccerDronePawn* AImperialMatchManager::GetSelectedDrone()const{return GetDrone(Selected);}
void AImperialMatchManager::SelectDrone(int32 Id)
{
    if(!Drones.IsValidIndex(Id))return;Selected=Id;
    if(auto* PC=Cast<AImperialPlayerController>(UGameplayStatics::GetPlayerController(this,0))){PC->Possess(Drones[Id]);PC->SetView(CameraView);}
}
void AImperialMatchManager::SelectNext(){SelectDrone((Selected+1)%FMath::Max(1,Drones.Num()));}
FString AImperialMatchManager::GetModeName()const
{
    switch(Mode){case EImperialMode::Mission:return TEXT("SDACS AUTONOMOUS");case EImperialMode::Hold:return TEXT("POSITION HOLD");case EImperialMode::Returning:return TEXT("RETURNING");default:return TEXT("MANUAL TRAINING");}
}
void AImperialMatchManager::Log(const FString& Message){Notice=Message;Events.Add(Message);if(Events.Num()>40)Events.RemoveAt(0);}
void AImperialMatchManager::LaunchSwarm()
{
    if(Rules.GetPhase()!=drone_soccer::Phase::Practice){Log(TEXT("Autonomous guidance is available in practice mode. Reset to practice first."));return;}
    if(!bConnected){Log(TEXT("SDACS bridge offline. Run ImperialWings.cmd to start the local planner."));return;}
    AdvanceGuidanceCommand();Mode=EImperialMode::Mission;
    for(auto& D:Drones){D->SetAutonomousEnabled(true);D->SetArmed(true);}
    Log(TEXT("Fleet autonomous: upstream SDACS APF targets, local Chaos motor forces."));
}
void AImperialMatchManager::HoldFleet()
{
    AdvanceGuidanceCommand();Mode=EImperialMode::Hold;
    for(auto& D:Drones)if(D->IsArmed()){D->SetAutonomousEnabled(true);D->SetAutonomousTarget(D->GetActorLocation());}
    Log(TEXT("Fleet holding current positions. Physics and collision remain active."));
}
void AImperialMatchManager::ReturnFleet()
{
    if(!bConnected){HoldFleet();Log(TEXT("SDACS offline: return unavailable. Holding locally; request RETURN after reconnect."));return;}
    AdvanceGuidanceCommand();Mode=EImperialMode::Returning;
    for(int32 I=0;I<Drones.Num();++I)if(Drones[I]->IsArmed()){Drones[I]->SetAutonomousEnabled(true);Drones[I]->SetAutonomousTarget(Drones[I]->GetActorLocation());}
    Log(TEXT("Fleet returning to launch pads for a controlled landing."));
}
void AImperialMatchManager::ManualSelected()
{
    if(auto* D=GetSelectedDrone()){AdvanceGuidanceCommand();D->SetAutonomousEnabled(false);D->SetArmed(false);Mode=EImperialMode::Manual;Log(TEXT("Selected aircraft disarmed. Current input source retained; low throttle is required to arm."));}
}
void AImperialMatchManager::ToggleArmSelected(){if(auto* D=GetSelectedDrone()){if(D->IsAutonomousEnabled())D->SetAutonomousEnabled(false);else D->SetArmed(!D->IsArmed());}}
void AImperialMatchManager::ResetMatch()
{
    AdvanceGuidanceCommand();
    for(auto& D:Drones)D->ResetDrone();Mode=EImperialMode::Manual;BlueScore=OrangeScore=0;Elapsed=0;Rules.Reset();
    for(int32 I=0;I<Drones.Num();++I){PreviousPositions[I]=Drones[I]->GetActorLocation();GoalReady[I]=true;}
    // The next generation resets only this session's planner state. An unordered
    // separate /reset request could otherwise clear a newer mission.
    Events.Empty();Log(TEXT("Training reset. All aircraft disarmed; scores cleared."));
}
void AImperialMatchManager::StartMatch()
{
    ResetMatch();Rules.StartMatch();ChangeView(1);Log(TEXT("FIDA Class 40 LOS match: 10-second ready signal, then 3-minute sets. Manual control."));
}
FString AImperialMatchManager::GetMatchStatus() const
{
    switch(Rules.GetPhase())
    {
    case drone_soccer::Phase::Ready:return TEXT("READY / MOTORS LOCKED");
    case drone_soccer::Phase::Playing:return FString::Printf(TEXT("SET %d / FIDA CLASS 40"),Rules.CurrentSet());
    case drone_soccer::Phase::Break:return TEXT("MAINTENANCE / 5 MIN");
    case drone_soccer::Phase::Finished:return TEXT("MATCH FINISHED");
    default:return TEXT("PRACTICE / FIDA CLASS 40");
    }
}
void AImperialMatchManager::ChangeView(int32 View){CameraView=FMath::Clamp(View,1,3);if(auto* PC=Cast<AImperialPlayerController>(UGameplayStatics::GetPlayerController(this,0)))PC->SetView(CameraView);}
void AImperialMatchManager::Tick(float Dt)
{
    Super::Tick(Dt);Elapsed+=Dt;SmoothFPS=FMath::Lerp(SmoothFPS,1.f/FMath::Max(.001f,Dt),.04f);
    for(int32 I=0;I<Drones.Num();++I){TArray<FVector> Sources;TArray<float> Thrusts;for(int32 J=0;J<Drones.Num();++J)if(J!=I&&Drones[J]->IsArmed()){Sources.Add(Drones[J]->GetActorLocation()/100.);Thrusts.Add(Drones[J]->GetTotalThrustNewtons());}Drones[I]->SetWakeSources(Sources,Thrusts);}
    ClosestSeparation=1000;Conflicts=0;
    for(int32 I=0;I<Drones.Num();++I)for(int32 J=I+1;J<Drones.Num();++J){float D=FVector::Distance(Drones[I]->GetActorLocation(),Drones[J]->GetActorLocation())/100.f;ClosestSeparation=FMath::Min(ClosestSeparation,D);if(D<.9f)++Conflicts;}
    if(Mode==EImperialMode::Returning)
    {
        bool Landed=true;
        for(int32 I=0;I<Drones.Num();++I)if(Drones[I]->IsArmed()){
            if(FVector::Dist2D(Drones[I]->GetActorLocation(),Homes[I])<45&&Drones[I]->GetActorLocation().Z<65&&Drones[I]->GetFlightSpeed()<.6){Drones[I]->SetAutonomousEnabled(false);Drones[I]->SetArmed(false);}
            else{Landed=false;}
        }
        if(Landed){AdvanceGuidanceCommand();Mode=EImperialMode::Manual;Log(TEXT("Fleet landing complete. All motors disarmed."));}
    }
    const double WallNow=FPlatformTime::Seconds();
    if(GuidanceWindow.Expired(WallNow))CancelPendingGuidance();
    if(!bRequestPending&&!bSmoke&&!bAngleSmoke&&(LastRequestWallTime==0||WallNow-LastRequestWallTime>=.1))RequestGuidance();
    if(bDemo&&!bDemoStarted&&bConnected&&Elapsed>2){bDemoStarted=true;LaunchSwarm();}
    if(bConnected&&WallNow-LastResponse>2){bConnected=false;if(Mode==EImperialMode::Mission||Mode==EImperialMode::Returning)HoldFleet();Log(TEXT("SDACS timeout. Local hold; reconnect does not resume mission or return."));}
    UpdateScoring();if(bSmoke&&!bSmokeDone)SmokeTick();
    if(bAngleSmoke&&!bSmokeDone)AngleSmokeTick();
    if(!CaptureFile.IsEmpty()&&Elapsed>CaptureAt&&!bCaptureRequested){bCaptureRequested=true;FScreenshotRequest::RequestScreenshot(CaptureFile,true,false);}
    if(bCaptureRequested&&Elapsed>CaptureAt+1&&FPaths::FileExists(CaptureFile))FPlatformMisc::RequestExit(false);
    if(FParse::Param(FCommandLine::Get(),TEXT("ImperialProfile"))&&Elapsed>3&&!bProfileWritten)
    {
        FrameTimes.Add(Dt*1000.);
        if(Elapsed>18&&FrameTimes.Num()>20)
        {
            FrameTimes.Sort();double Total=0;for(double T:FrameTimes)Total+=T;
            FVector2D Size(0,0);if(auto* V=GetWorld()->GetGameViewport())V->GetViewportSize(Size);
            FString Profile=FString::Printf(TEXT("{\"samples\":%d,\"average_frame_ms\":%.3f,\"p95_frame_ms\":%.3f,\"average_fps\":%.2f,\"res_x\":%.0f,\"res_y\":%.0f,\"note\":\"rendered game frame pacing; not USB input latency\"}\n"),FrameTimes.Num(),Total/FrameTimes.Num(),FrameTimes[FMath::Min(FrameTimes.Num()-1,FMath::FloorToInt(FrameTimes.Num()*.95))],1000.*FrameTimes.Num()/Total,Size.X,Size.Y);
            AttachVerificationIdentity(Profile);
            FFileHelper::SaveStringToFile(Profile,*FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("render-profile.json")));bProfileWritten=true;
        }
    }
}
void AImperialMatchManager::RequestGuidance()
{
    const double Now=FPlatformTime::Seconds();ImperialGuidance::Ticket Ticket;
    if(!GuidanceWindow.Begin(Now,Ticket))return;
    const double Interval=LastRequestWallTime>0?Now-LastRequestWallTime:.1;
    auto O=MakeShared<FJsonObject>();O->SetNumberField(TEXT("dt"),FMath::Clamp(Interval,.001,.5));
    O->SetNumberField(TEXT("sample_interval_s"),Interval);
    O->SetNumberField(TEXT("protocol_version"),ImperialGuidance::ProtocolVersion);
    O->SetStringField(TEXT("session_id"),GuidanceSession);
    O->SetNumberField(TEXT("command_id"),static_cast<double>(Ticket.Command));
    O->SetNumberField(TEXT("request_id"),static_cast<double>(Ticket.Request));
    O->SetStringField(TEXT("mode"),GuidanceMode());
    TArray<TSharedPtr<FJsonValue>> List;
    for(int32 I=0;I<Drones.Num();++I){auto D=MakeShared<FJsonObject>();D->SetNumberField(TEXT("id"),I);D->SetArrayField(TEXT("position_m"),JsonVector(Drones[I]->GetActorLocation()/100.));D->SetArrayField(TEXT("velocity_mps"),JsonVector(Drones[I]->GetBodyMesh()->GetPhysicsLinearVelocity()/100.));D->SetArrayField(TEXT("home_m"),JsonVector(Homes[I]/100.));List.Add(MakeShared<FJsonValueObject>(D));}
    O->SetArrayField(TEXT("drones"),List);FString Body;auto Writer=TJsonWriterFactory<>::Create(&Body);FJsonSerializer::Serialize(O,Writer);
    auto R=FHttpModule::Get().CreateRequest();R->SetURL(GuidanceUrl+TEXT("/step"));R->SetVerb(TEXT("POST"));R->SetTimeout(1.f);R->SetHeader(TEXT("Content-Type"),TEXT("application/json"));R->SetContentAsString(Body);R->OnProcessRequestComplete().BindUObject(this,&AImperialMatchManager::OnGuidance,Ticket);
    PendingGuidance=R;bRequestPending=R->ProcessRequest();LastRequestWallTime=Now;
    if(!bRequestPending){PendingGuidance.Reset();GuidanceWindow.Abort();}
}
void AImperialMatchManager::OnGuidance(FHttpRequestPtr Request,FHttpResponsePtr Response,bool bSuccess,ImperialGuidance::Ticket Ticket)
{
    if(Request!=PendingGuidance||!GuidanceWindow.IsCurrent(Ticket))return;
    PendingGuidance.Reset();bRequestPending=false;
    if(!GuidanceWindow.Complete(Ticket,FPlatformTime::Seconds())||!bSuccess||!Response.IsValid()||Response->GetResponseCode()!=200)return;
    TSharedPtr<FJsonObject> O;auto Reader=TJsonReaderFactory<>::Create(Response->GetContentAsString());if(!FJsonSerializer::Deserialize(Reader,O)||!O.IsValid())return;
    double Protocol=0,Command=0,Sequence=0;FString Session,ResponseMode;
    if(!O->TryGetNumberField(TEXT("protocol_version"),Protocol)||Protocol!=ImperialGuidance::ProtocolVersion||
       !O->TryGetStringField(TEXT("session_id"),Session)||Session!=GuidanceSession||
       !O->TryGetNumberField(TEXT("command_id"),Command)||Command!=static_cast<double>(Ticket.Command)||
       !O->TryGetNumberField(TEXT("request_id"),Sequence)||Sequence!=static_cast<double>(Ticket.Request)||
       !O->TryGetStringField(TEXT("mode"),ResponseMode)||ResponseMode!=GuidanceMode())return;
    FString Source;if(!O->TryGetStringField(TEXT("source"),Source)||Source!=TEXT("swarm-drone-atc"))return;
    const TArray<TSharedPtr<FJsonValue>>* Targets=nullptr;if(!O->TryGetArrayField(TEXT("targets"),Targets)||Targets->Num()!=Drones.Num())return;
    TArray<FVector> Goals;Goals.SetNum(Drones.Num());TSet<int32> Seen;
    for(auto T:*Targets){if(!T.IsValid()||T->Type!=EJson::Object)return;auto D=T->AsObject();double Number=0;if(!D.IsValid()||!D->TryGetNumberField(TEXT("id"),Number)||!FMath::IsFinite(Number)||Number<0||Number>=Drones.Num()||Number!=FMath::FloorToDouble(Number))return;const int32 Id=static_cast<int32>(Number);if(Seen.Contains(Id))return;FVector V;if(!ReadVector(D,TEXT("target_m"),V)||FMath::Abs(V.X)>7.8||FMath::Abs(V.Y)>3.3||V.Z<.24||V.Z>4.3)return;Goals[Id]=V*100;Seen.Add(Id);}
    bool WasConnected=bConnected;bConnected=true;LastResponse=FPlatformTime::Seconds();O->TryGetStringField(TEXT("source_commit"),SourceCommit);
    if(!WasConnected)Log(TEXT("SDACS upstream APF connected. Session and command verified."));
    if(Mode==EImperialMode::Mission||Mode==EImperialMode::Returning)for(int32 I=0;I<Drones.Num();++I)if(Drones[I]->IsAutonomousEnabled())Drones[I]->SetAutonomousTarget(Goals[I]);
}
void AImperialMatchManager::UpdateScoring()
{
    std::array<drone_soccer::Vec3,10> Poses;
    for(int32 I=0;I<Drones.Num();++I){FVector P=Drones[I]->GetActorLocation()/100.;Poses[I]={P.X,P.Y,P.Z};}
    const int Before=Rules.Score(0)+Rules.Score(1);const auto Phase=Rules.GetPhase();
    Rules.Update(Poses,GetWorld()->GetDeltaSeconds());
    if(Rules.Score(0)+Rules.Score(1)>Before)Log(FString::Printf(TEXT("GOAL! Blue %d : %d Red. Scoring team must all retreat."),Rules.Score(0),Rules.Score(1)));
    if(auto* Arena=Cast<APrototypeArenaBuilder>(UGameplayStatics::GetActorOfClass(GetWorld(),APrototypeArenaBuilder::StaticClass()))){Arena->SetGoalSignal(1,Rules.IsOffside(0));Arena->SetGoalSignal(0,Rules.IsOffside(1));}
    if(Phase==drone_soccer::Phase::Playing&&Rules.GetPhase()!=Phase)ReturnFleet();
}
bool AImperialMatchManager::ExportTelemetry()
{
    FString Dir=FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("Reports"));IFileManager::Get().MakeDirectory(*Dir,true);
    FString File=FPaths::Combine(Dir,FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M%S"))+FString::Printf(TEXT("-%llu-fleet.csv"),FPlatformTime::Cycles64()));
    auto* Radio=GetWorld()->GetGameInstance()->GetSubsystem<UImperialRadioSubsystem>();
    const auto* Device=Radio?Radio->GetSelectedDevice():nullptr;
    const FString RadioId=Device?FString::Printf(TEXT("%04X:%04X"),Device->ManufacturerId,Device->ProductId):TEXT("NONE");
    auto* Arena=Cast<APrototypeArenaBuilder>(UGameplayStatics::GetActorOfClass(GetWorld(),APrototypeArenaBuilder::StaticClass()));
    auto* Pilot=Cast<AImperialPlayerController>(UGameplayStatics::GetPlayerController(this,0));
    const FVector Station=Pilot?Pilot->GetPilotStationPosition()/100.:FVector::ZeroVector;
    const auto N=[](double Value){return FString::Printf(TEXT("%.6f"),Value);};
    const auto B=[](bool Value){return Value?TEXT("1"):TEXT("0");};
    FString CSV=TEXT("id,team,armed,autonomous,x_m,y_m,z_m,speed_mps,throttle,mode,sdacs_connected,input_source,selected,camera_view,radio_connected,radio_calibrated,radio_enabled,battery_soc,battery_v,net_deflection_m,radio_setup_phase,radio_setup_channel,radio_setup_samples,radio_device_id,build_version,build_commit,build_source_sha256,build_utc,guidance_session,guidance_command,flight_mode,tilt_deg,roll_deg,pitch_deg,yaw_deg,angle_limit_deg,role,home_x_m,home_y_m,pad_x_m,pad_y_m,pilot_x_m,pilot_y_m\n");
    for(int32 I=0;I<Drones.Num();++I)
    {
        auto* D=Drones[I].Get();const FVector P=D->GetActorLocation()/100.;
        const FRotator Rotation=D->GetBodyMesh()->GetComponentRotation();
        FVector Pad(-1000000);if(Arena)Arena->GetStartPadPosition(I,Pad);Pad/=100.;
        const TArray<FString> Row={
            FString::Printf(TEXT("IW-%02d"),I+1),I<5?TEXT("BLUE"):TEXT("RED"),B(D->IsArmed()),B(D->IsAutonomousEnabled()),
            N(P.X),N(P.Y),N(P.Z),N(D->GetFlightSpeed()),N(D->GetThrottle()),GetModeName(),B(bConnected),D->GetInputSource(),B(I==Selected),FString::FromInt(CameraView),
            B(Radio&&Radio->IsConnected()),B(Radio&&Radio->IsCalibrated()),B(Radio&&Radio->IsEnabled()),N(D->GetBatteryPercent()/100.f),N(D->GetBatteryVoltage()),N(D->GetNetDeflection()),
            FString::FromInt(Radio?static_cast<int32>(Radio->GetGuidedPhase()):0),FString::FromInt(Radio?Radio->GetGuidedChannel():0),FString::FromInt(Radio?Radio->GetCaptureSampleCount():0),RadioId,
            ImperialBuildInfo::Version(),ImperialBuildInfo::Commit(),ImperialBuildInfo::SourceHash(),ImperialBuildInfo::BuiltAt(),GuidanceSession,FString::Printf(TEXT("%llu"),static_cast<uint64>(GuidanceWindow.Command())),
            D->GetFlightModeName(),N(D->GetTiltDegrees()),N(Rotation.Roll),N(Rotation.Pitch),N(Rotation.Yaw),N(D->GetAngleLimitDegrees()),ImperialSoccerLayout::RoleName(D->GetPlayerRole()),
            N(Homes[I].X/100.),N(Homes[I].Y/100.),N(Pad.X),N(Pad.Y),N(Station.X),N(Station.Y)};
        CSV+=FString::Join(Row,TEXT(","))+TEXT("\n");
    }
    bool Ok=FFileHelper::SaveStringToFile(CSV,*File,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);Log(Ok?TEXT("Telemetry CSV exported to Saved/Reports."):TEXT("Telemetry export failed."));return Ok;
}
void AImperialMatchManager::AngleSmokeTick()
{
    // Explicit initial-pose and non-USB control fixture. All later movement is
    // motor forces/Chaos. Vertical compensation isolates the attitude test.
    if(SmokePhase==0&&Elapsed>.5)
    {
        for(int32 I=0;I<Drones.Num();++I)
        {
            auto* D=Drones[I].Get();D->ResetDrone();D->SetAngleVerificationInput(FVector::ZeroVector,0);
            D->GetBodyMesh()->SetWorldLocationAndRotation(FVector(Homes[I].X,Homes[I].Y,180),FRotator(20,I<5?0:180,15),false,nullptr,ETeleportType::TeleportPhysics);
            D->SetArmed(true);
        }
        SmokePhase=1;AnglePhaseStart=Elapsed;
    }
    if(SmokePhase==0)return;
    float WorstTilt=0;bool Airborne=true;
    for(auto& D:Drones)
    {
        const FVector Velocity=D->GetBodyMesh()->GetPhysicsLinearVelocity()/100.;
        const double Acceleration=FMath::Clamp((1.8-D->GetAltitudeMeters())*3.-Velocity.Z*2.,-3.,3.);
        const double Thrust=.9*(9.81+Acceleration)/(4*FMath::Max(.45,D->GetBodyMesh()->GetUpVector().Z));
        const double VoltageFactor=FMath::Clamp(FMath::Square(D->GetBatteryVoltage()/14.8),.2,1.5);
        const float PWM=FMath::Sqrt(FMath::Clamp((Thrust/VoltageFactor-.12)/7.88,0.,1.));
        D->SetAngleVerificationInput(SmokePhase==2?FVector(0,.5,0):FVector::ZeroVector,PWM);
        WorstTilt=FMath::Max(WorstTilt,D->GetTiltDegrees());
        Airborne&=D->GetAltitudeMeters()>.7&&D->GetAltitudeMeters()<3.7;
    }
    AnglePeak=FMath::Max(AnglePeak,WorstTilt);
    const double Age=Elapsed-AnglePhaseStart;
    if(SmokePhase==1&&Age>1.8){bAngleInitialLevel=WorstTilt<3&&Airborne;SmokePhase=2;AnglePhaseStart=Elapsed;}
    else if(SmokePhase==2)
    {
        AngleInputPeak=FMath::Max(AngleInputPeak,WorstTilt);
        if(Age>.65){SmokePhase=3;AnglePhaseStart=Elapsed;}
    }
    else if(SmokePhase==3&&Age>1.8)
    {
        AngleReleaseWorst=WorstTilt;bAngleReleasedLevel=WorstTilt<3&&Airborne;
        const bool Valid=bAngleInitialLevel&&bAngleReleasedLevel&&AngleInputPeak>8&&AngleInputPeak<40&&AnglePeak<45&&Drones.Num()==10;
        auto Result=MakeShared<FJsonObject>();Result->SetBoolField(TEXT("ok"),Valid);
        Result->SetNumberField(TEXT("drones"),Drones.Num());Result->SetBoolField(TEXT("initial_self_level"),bAngleInitialLevel);
        Result->SetNumberField(TEXT("stick_tilt_peak_deg"),AngleInputPeak);Result->SetNumberField(TEXT("overall_tilt_peak_deg"),AnglePeak);
        Result->SetNumberField(TEXT("released_worst_tilt_deg"),AngleReleaseWorst);Result->SetBoolField(TEXT("released_self_level"),bAngleReleasedLevel);
        Result->SetStringField(TEXT("input_source"),TEXT("explicit non-USB verification fixture; vertical compensation isolates angle control"));
        FString Text;FJsonSerializer::Serialize(Result,TJsonWriterFactory<>::Create(&Text));AttachVerificationIdentity(Text);
        FFileHelper::SaveStringToFile(Text,*(SmokeResultFile.IsEmpty()?FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("angle-smoke.json")):SmokeResultFile));
        ExportTelemetry();bSmokeDone=true;UE_LOG(LogTemp,Display,TEXT("IMPERIAL_ANGLE_SMOKE %s"),*Text);
        FPlatformMisc::RequestExitWithStatus(false,Valid?0:1);
    }
}
void AImperialMatchManager::SmokeTick()
{
    auto* D=GetDrone(0);if(!D)return;
    if(Elapsed>1&&SmokePhase==0){D->SetAutonomousEnabled(true);D->SetAutonomousTarget(FVector(-550,-300,220));SmokePhase=1;}
    if(SmokePhase==1){if(Elapsed>9)D->SetAutonomousTarget(FVector(-350,-150,220));SmokeMaxHeight=FMath::Max(SmokeMaxHeight,(float)D->GetActorLocation().Z);if(Elapsed>12)SmokeMinHeight=FMath::Min(SmokeMinHeight,(float)D->GetActorLocation().Z);}
    if(Elapsed>20&&SmokePhase==1){SmokeHoverError=FVector::Distance(D->GetActorLocation(),FVector(-350,-150,220));bool Stable=SmokeHoverError<70&&SmokeMaxHeight<450&&SmokeMinHeight>120;D->SetAutonomousEnabled(false);D->SetArmed(false);SmokePhase=Stable?2:9;UE_LOG(LogTemp,Display,TEXT("IMPERIAL_HOVER error_cm=%.2f max_z=%.2f min_z=%.2f stable=%d"),SmokeHoverError,SmokeMaxHeight,SmokeMinHeight,Stable);}
    if(Elapsed>24&&SmokePhase==2){bool Floor=D->GetActorLocation().Z>15&&D->GetActorLocation().Z<30;D->ResetDrone();D->GetBodyMesh()->SetPhysicsLinearVelocity(FVector(-2500,0,0));SmokePhase=Floor?3:9;}
    if(Elapsed>27&&SmokePhase==3)
    {
        bSmokeFloorWall=D->GetActorLocation().X>-805&&D->GetActorLocation().Z>15&&D->GetActorLocation().Z<30;
        // Explicit collision fixture: one initial placement and a 5 m/s net impact.
        // No position correction is used during subsequent flight or contact.
        D->ResetDrone();D->GetBodyMesh()->SetWorldLocationAndRotation(FVector(0,-200,220),FRotator::ZeroRotator,false,nullptr,ETeleportType::TeleportPhysics);
        D->GetBodyMesh()->SetPhysicsLinearVelocity(FVector(0,-500,0));SmokeNetMinY=-200;SmokePhase=4;
    }
    if(SmokePhase==4)
    {
        SmokeNetPeak=FMath::Max(SmokeNetPeak,D->GetNetDeflection());SmokeNetMinY=FMath::Min(SmokeNetMinY,(float)D->GetActorLocation().Y);
        SmokeNetRebound=FMath::Max(SmokeNetRebound,(float)D->GetBodyMesh()->GetPhysicsLinearVelocity().Y);
        if(auto* N=Cast<AImperialElasticNet>(UGameplayStatics::GetActorOfClass(GetWorld(),AImperialElasticNet::StaticClass())))SmokeNetVisualPeak=FMath::Max(SmokeNetVisualPeak,N->GetMaxDeflection());
        if(Elapsed>30)
        {
            bSmokeNet=SmokeNetPeak>.001f&&SmokeNetPeak<=.2001f&&SmokeNetVisualPeak>.001f&&SmokeNetVisualPeak<=.2001f&&SmokeNetRebound>5&&SmokeNetMinY>-351;
            UE_LOG(LogTemp,Display,TEXT("IMPERIAL_NET force_deflection_m=%.4f visual_deflection_m=%.4f rebound_cm_s=%.2f min_y_cm=%.2f pass=%d"),SmokeNetPeak,SmokeNetVisualPeak,SmokeNetRebound,SmokeNetMinY,bSmokeNet);
            Rules.Reset();D->ResetDrone();D->GetBodyMesh()->SetWorldLocationAndRotation(FVector(500,0,300),FRotator::ZeroRotator,false,nullptr,ETeleportType::TeleportPhysics);
            D->SetAutonomousEnabled(true);D->SetAutonomousTarget(FVector(500,0,300));SmokePhase=5;
        }
    }
    if(SmokePhase==5)
    {
        if(Elapsed>33)D->SetAutonomousTarget(FVector(725,0,300));
        if(D->GetActorLocation().X>610&&D->GetActorLocation().X<690)UE_LOG(LogTemp,Display,TEXT("IMPERIAL_GOAL_TRACE elapsed=%.3f pose_cm=%s score=%d offside=%d"),Elapsed,*D->GetActorLocation().ToString(),Rules.Score(0),Rules.IsOffside(0));
    }
    if(Elapsed<43)return;bSmokeDone=true;
    bSmokeGoal=SmokePhase==5&&Rules.Score(0)==1&&Rules.Score(1)==0&&Rules.IsOffside(0)&&D->GetActorLocation().X>678;
    const bool CSV=ExportTelemetry();const bool Valid=SmokePhase==5&&bSmokeFloorWall&&bSmokeNet&&bSmokeGoal&&Drones.Num()==10&&CSV;
    FString Result=FString::Printf(TEXT("{\"ok\":%s,\"drones\":%d,\"hover_error_cm\":%.3f,\"hover_max_z_cm\":%.3f,\"hover_min_z_cm\":%.3f,\"floor_wall_contact\":%s,\"net_contact\":%s,\"net_peak_m\":%.5f,\"net_visible_peak_m\":%.5f,\"net_rebound_mps\":%.4f,\"striker_physical_goal\":%s,\"blue_score\":%d,\"csv_export\":%s}\n"),Valid?TEXT("true"):TEXT("false"),Drones.Num(),SmokeHoverError,SmokeMaxHeight,SmokeMinHeight,bSmokeFloorWall?TEXT("true"):TEXT("false"),bSmokeNet?TEXT("true"):TEXT("false"),SmokeNetPeak,SmokeNetVisualPeak,SmokeNetRebound/100,bSmokeGoal?TEXT("true"):TEXT("false"),Rules.Score(0),CSV?TEXT("true"):TEXT("false"));
    AttachVerificationIdentity(Result);
    FFileHelper::SaveStringToFile(Result,*(SmokeResultFile.IsEmpty()?FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("physics-smoke.json")):SmokeResultFile));
    UE_LOG(LogTemp,Display,TEXT("IMPERIAL_PHYSICS_SMOKE_%s %s"),Valid?TEXT("PASS"):TEXT("FAIL"),*Result);FPlatformMisc::RequestExitWithStatus(false,Valid?0:1);
}
