#include "ImperialAIPracticeVerifier.h"
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

AImperialAIPracticeVerifier::AImperialAIPracticeVerifier(){PrimaryActorTick.bCanEverTick=true;PrimaryActorTick.TickGroup=TG_PostPhysics;}
void AImperialAIPracticeVerifier::Check(const TCHAR* Name,bool Passed,double Value)
{
    auto R=MakeShared<FJsonObject>();R->SetNumberField(TEXT("case"),Case);R->SetStringField(TEXT("check"),Name);R->SetBoolField(TEXT("ok"),Passed);R->SetNumberField(TEXT("value"),Value);Checks.Add(MakeShared<FJsonValueObject>(R));bOk&=Passed;
}
void AImperialAIPracticeVerifier::Finish()
{
    auto R=MakeShared<FJsonObject>();R->SetBoolField(TEXT("ok"),bOk);R->SetArrayField(TEXT("checks"),Checks);R->SetObjectField(TEXT("build"),ImperialBuildInfo::Json());
    R->SetNumberField(TEXT("min_game_dt_s"),MinDt);R->SetNumberField(TEXT("max_game_dt_s"),MaxDt);R->SetNumberField(TEXT("simulated_seconds"),Total);
    R->SetNumberField(TEXT("completed_flight_scenarios"),Case);
    R->SetStringField(TEXT("scope"),TEXT("Native menu API and actual Chaos defensive flights / landings, no USB input or human difficulty rating."));
    FString File,Json;FParse::Value(FCommandLine::Get(),TEXT("ImperialAIPracticeResult="),File);FJsonSerializer::Serialize(R,TJsonWriterFactory<>::Create(&Json));
    const bool Saved=FFileHelper::SaveStringToFile(Json,*File,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    SetActorTickEnabled(false);FPlatformMisc::RequestExitWithStatus(false,bOk&&Saved?0:1);
}
void AImperialAIPracticeVerifier::Tick(float Dt)
{
    Super::Tick(Dt);Age+=Dt;Total+=Dt;MinDt=FMath::Min(MinDt,static_cast<double>(Dt));MaxDt=FMath::Max(MaxDt,static_cast<double>(Dt));
    if(Total>650){Check(TEXT("bounded runtime"),false);Finish();return;}
    auto* M=Cast<AImperialMatchManager>(UGameplayStatics::GetActorOfClass(GetWorld(),AImperialMatchManager::StaticClass()));
    auto* PC=Cast<AImperialPlayerController>(UGameplayStatics::GetPlayerController(this,0));
    auto* H=PC?Cast<AImperialHUD>(PC->GetHUD()):nullptr;
    if(!M||!H||Age<.2)return;
    if(Stage==0)
    {
        FParse::Value(FCommandLine::Get(),TEXT("ImperialAIPracticeCapture="),Capture);
        if(GEngine)GEngine->SetMaxFPS(Capture.IsEmpty()?0:60);
        M->ResetMatch();M->ChangeView(1);if(H->IsRadioPanelOpen())H->ToggleRadioPanel();
        Check(TEXT("invalid count difficulty and zero seed rejected"),!M->StartAIPractice({2,0,1})&&!M->StartAIPractice({3,3,1})&&!M->StartAIPractice({3,0,0}));
        M->StartMatch();Check(TEXT("official match rejects AI"),!M->StartAIPractice({3,0,1})&&!M->IsAutonomyAllowed());M->ResetMatch();
        M->ChangeView(4);Check(TEXT("audience view rejects AI"),PC->IsReadOnlyView()&&!M->StartAIPractice({3,0,1}));M->ChangeView(1);
        Stage=1;Age=0;return;
    }
    const int32 Count=Case<3?3:4,Level=Case%3;
    if(Stage==1)
    {
        if(Case==0)
        {
            const bool Armed=M->GetDrone(0)->SetArmed(true);
            Check(TEXT("armed player blocks AI start"),Armed&&!M->StartAIPractice({3,0,1})&&!M->IsAIPracticeActive());M->GetDrone(0)->SetArmed(false);
        }
        M->ResetMatch();
        if(!H->IsAIPracticePanelOpen())H->ToggleAIPracticePanel();
        H->NotifyHitBoxClick(Count==3?TEXT("AI_Count3"):TEXT("AI_Count4"));
        H->NotifyHitBoxClick(Level==0?TEXT("AI_Easy"):(Level==1?TEXT("AI_Medium"):TEXT("AI_Hard")));
        H->NotifyHitBoxClick(TEXT("Auto"));H->NotifyHitBoxClick(TEXT("StartMatch"));H->NotifyHitBoxClick(TEXT("ArmSelected"));
        bool Quiet=true;for(auto D:M->GetDrones())Quiet&=!D->IsArmed();
        Check(TEXT("practice modal blocks underlying flight and match buttons"),Quiet&&M->IsAutonomyAllowed()&&!M->IsAIPracticeActive());
        H->NotifyHitBoxClick(TEXT("AI_Start"));
        Check(TEXT("UI starts requested count difficulty and closes modal"),M->IsAIPracticeActive()&&!H->IsPilotMenuOpen()&&M->GetAISettings().Count==Count&&M->GetAISettings().Difficulty==Level);
        M->SelectDrone(6);M->LaunchSwarm();M->HoldFleet();M->StartMatch();
        Check(TEXT("AI ownership survives conflicting actions; player remains manual"),M->GetSelected()==0&&M->IsAIPracticeActive()&&!M->IsAILanding()&&!M->GetDrone(0)->IsAutonomousEnabled()&&!M->GetDrone(0)->IsArmed());
        if(Case==0){M->ToggleFlightRecording();Check(TEXT("practice flight recording starts"),M->IsRecordingFlight());}
        Age=0;Stage=2;bFinite=bCountSafe=true;MaxSpeed=MaxHeight=MaxError=0;MinAirborne=100;return;
    }
    if(Stage==2)
    {
        for(int32 I=0;I<10;++I)
        {
            auto* D=M->GetDrone(I);const FVector P=D->GetActorLocation(),V=D->GetBodyMesh()->GetPhysicsLinearVelocity();
            const bool Active=I>=6&&I<6+Count;
            bCountSafe&=D->IsAutonomousEnabled()==Active&&D->IsArmed()==Active;
            bFinite&=!P.ContainsNaN()&&!V.ContainsNaN()&&FMath::Abs(P.X)<810&&FMath::Abs(P.Y)<360&&P.Z>=15&&P.Z<445;
            if(Active){MaxSpeed=FMath::Max(MaxSpeed,static_cast<double>(D->GetFlightSpeed()));MaxHeight=FMath::Max(MaxHeight,P.Z/100.);if(Age>15){MinAirborne=FMath::Min(MinAirborne,P.Z/100.);MaxError=FMath::Max(MaxError,FVector::Distance(P,D->GetAutonomousTarget())/100.);}}
        }
        if(!Capture.IsEmpty()&&Case==0&&!bCaptured&&Age>21)
        {
            H->ToggleAIPracticePanel();FScreenshotRequest::RequestScreenshot(Capture,true,false);bCaptured=true;
        }
        if(Age<25)return;
        Check(TEXT("exact AI count; all other motors remain disarmed"),bCountSafe);
        Check(TEXT("actual bodies remain finite inside arena"),bFinite);
        Check(TEXT("all defenders airborne after approach"),MinAirborne>.7,MinAirborne);
        Check(TEXT("defenders approach moving tactical targets"),MaxError<2.5,MaxError);
        Check(TEXT("bounded actual flight speed"),MaxSpeed<8,MaxSpeed);
        Check(TEXT("tactical decisions advance"),M->GetAIDecisions()>5,M->GetAIDecisions());
        if(!H->IsAIPracticePanelOpen())H->ToggleAIPracticePanel();
        const auto Previous=M->GetAISettings();H->NotifyHitBoxClick(TEXT("AI_Count4"));H->NotifyHitBoxClick(TEXT("AI_Hard"));H->NotifyHitBoxClick(TEXT("AI_Seed"));
        Check(TEXT("running configuration locked"),M->GetAISettings().Count==Previous.Count&&M->GetAISettings().Difficulty==Previous.Difficulty&&M->GetAISettings().Seed==Previous.Seed);
        H->NotifyHitBoxClick(TEXT("AI_Stop"));H->NotifyHitBoxClick(TEXT("AI_Close"));
        Check(TEXT("stop begins landing without teleport or instant motor cut"),M->IsAILanding()&&M->GetDrone(6)->IsArmed()&&M->GetDrone(6)->GetActorLocation().Z>70);
        Stage=3;Age=0;return;
    }
    if(Stage==3)
    {
        if(M->IsAIPracticeActive()&&Age<30)return;
        bool Landed=!M->IsAIPracticeActive();for(int32 I=6;I<6+Count;++I)Landed&=!M->GetDrone(I)->IsArmed()&&!M->GetDrone(I)->IsAutonomousEnabled()&&M->GetDrone(I)->GetActorLocation().Z<32;
        Check(TEXT("all AI landed and disarmed within 30 seconds"),Landed,Age);
        if(Case==0)M->ToggleFlightRecording();
        M->ResetMatch();M->StartMatch();bool Quiet=true;for(auto D:M->GetDrones())Quiet&=!D->IsArmed()&&!D->IsAutonomousEnabled();
        Check(TEXT("reset followed by official start leaves all motors off"),Quiet&&!M->IsAutonomyAllowed());M->ResetMatch();
        ++Case;if(!Capture.IsEmpty()){Finish();return;}
        Stage=Case==6?4:1;Age=0;return;
    }
    if(Stage==4)
    {
        Check(TEXT("restart for timeout test"),M->StartAIPractice({3,0,12345}));Stage=5;Age=0;return;
    }
    if(Stage==5&&Age>=180.2)
    {
        Check(TEXT("three-minute limit initiates controlled landing"),M->IsAILanding());
        M->ResetMatch();bool Quiet=true;for(auto D:M->GetDrones())Quiet&=!D->IsArmed()&&!D->IsAutonomousEnabled();
        Check(TEXT("explicit reset clears active AI and motors"),Quiet&&!M->IsAIPracticeActive());Finish();
    }
}
