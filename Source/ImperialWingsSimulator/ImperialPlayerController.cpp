#include "ImperialPlayerController.h"
#include "ImperialMatchManager.h"
#include "SoccerDronePawn.h"
#include "ImperialHUD.h"
#include "ImperialSoccerLayout.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/InputComponent.h"
#include "Kismet/GameplayStatics.h"
#include "InputCoreTypes.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Misc/Parse.h"
#include "Misc/CommandLine.h"

namespace
{
void ConfigureSpectatorCamera(UCameraComponent* Camera,float FieldOfView)
{
    Camera->bConstrainAspectRatio=false;Camera->SetFieldOfView(FieldOfView);
    auto& Settings=Camera->PostProcessSettings;
    Settings.bOverride_AutoExposureMethod=true;Settings.AutoExposureMethod=AEM_Manual;
    Settings.bOverride_AutoExposureApplyPhysicalCameraExposure=true;Settings.AutoExposureApplyPhysicalCameraExposure=false;
    Settings.bOverride_AutoExposureBias=true;Settings.AutoExposureBias=-4.f;
    Settings.bOverride_MotionBlurAmount=true;Settings.MotionBlurAmount=0;
}
}
void AImperialPlayerController::BeginPlay()
{
    Super::BeginPlay(); bShowMouseCursor=true; bEnableClickEvents=false;bAutoManageActiveCameraTarget=false;
    Match=Cast<AImperialMatchManager>(UGameplayStatics::GetActorOfClass(GetWorld(),AImperialMatchManager::StaticClass()));
    ArenaCamera=GetWorld()->SpawnActor<ACameraActor>();
    FVector Location(1750,-2250,1180),Target(0,30,180);
    ArenaCamera->SetActorLocationAndRotation(Location,(Target-Location).Rotation());
    ConfigureSpectatorCamera(ArenaCamera->GetCameraComponent(),60);
    PilotCamera=GetWorld()->SpawnActor<ACameraActor>();
    ConfigureSpectatorCamera(PilotCamera->GetCameraComponent(),PilotFieldOfView);
    UpdatePilotCamera();ConfigureInput();SetView(Match?Match->GetView():1);
}
void AImperialPlayerController::ConfigureInput()
{
    bShowMouseCursor=true;bEnableClickEvents=false;
    SetInputMode(FInputModeGameAndUI().SetHideCursorDuringCapture(false).SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock));
    // NoCapture suppresses mouse-down InputKey in a standalone game viewport.
    // Capture only for the press, keep the visible cursor unlocked and unhidden.
    if(auto* V=GetWorld()->GetGameViewport()){V->SetMouseCaptureMode(EMouseCaptureMode::CaptureDuringMouseDown);V->SetMouseLockMode(EMouseLockMode::DoNotLock);V->SetHideCursorDuringCapture(false);}
}
void AImperialPlayerController::PlayerTick(float Dt)
{
    Super::PlayerTick(Dt);
    if(Match&&!bPossessed&&Match->GetSelectedDrone()) {Possess(Match->GetSelectedDrone());bPossessed=true;SetView(Match->GetView());}
    const auto* HUD=Cast<AImperialHUD>(GetHUD());
    const bool RadioOpen=HUD&&HUD->IsRadioPanelOpen();
    float X=0,Y=0;
    if(ActiveView==1&&!RadioOpen&&IsInputKeyDown(EKeys::RightMouseButton)&&GetMousePosition(X,Y))
    {
        const FVector2D Pointer(X,Y);
        // Use visible cursor movement rather than high-precision captured mouse axes.
        if(bHeadDragging)
        {
            const FVector2D Delta=Pointer-LastHeadPointer;
            PilotHeadYaw=FMath::Clamp(PilotHeadYaw+static_cast<float>(Delta.X)*.18f,-60.f,60.f);
            PilotHeadPitch=FMath::Clamp(PilotHeadPitch-static_cast<float>(Delta.Y)*.14f,-25.f,40.f);
        }
        else if(!HUD||!HUD->IsPointerOverUI(Pointer))bHeadDragging=true;
        LastHeadPointer=Pointer;
    }
    else bHeadDragging=false;
    if(ActiveView==1)UpdatePilotCamera();
    if(ActiveView==3&&ArenaCamera&&Match&&Match->GetSelectedDrone()&&FParse::Param(FCommandLine::Get(),TEXT("ImperialShowcase")))
    {
        const FVector Centre=Match->GetSelectedDrone()->GetActorLocation();
        const FVector Location=Centre+FVector(80,-90,45);
        ArenaCamera->SetActorLocationAndRotation(Location,(Centre+FVector(0,0,1)-Location).Rotation());
        ArenaCamera->GetCameraComponent()->SetFieldOfView(45);
    }
    // View 2 is an external training observer, even if legacy pawn camera input is pressed.
    if(ActiveView==2&&Match&&Match->GetSelectedDrone())Match->GetSelectedDrone()->SetChaseView(true);
}
void AImperialPlayerController::UpdatePilotCamera()
{
    if(!PilotCamera)return;
    const int32 Team=Match&&Match->GetSelected()>=5?1:0;
    if(Team!=PilotTeam){PilotTeam=Team;PilotHeadYaw=0;PilotHeadPitch=0;bHeadDragging=false;}
    const int32 Slot=Match?Match->GetSelected()%5:0;
    const FVector Station(Team==0?-950.f:950.f,ImperialSoccerLayout::StartPosition(Team,Slot).Y,170);
    const FVector OpponentGoal(Team==0?650.f:-650.f,0,170);
    const FRotator Facing=(OpponentGoal-Station).Rotation();
    PilotCamera->SetActorLocationAndRotation(Station,FRotator(Facing.Pitch+PilotHeadPitch,Facing.Yaw+PilotHeadYaw,0));
    PilotCamera->GetCameraComponent()->SetFieldOfView(PilotFieldOfView);
}
void AImperialPlayerController::SetView(int32 View)
{
    if(!Match) return;
    ActiveView=FMath::Clamp(View,1,3);bHeadDragging=false;ConfigureInput();
    if(ActiveView==1&&PilotCamera){UpdatePilotCamera();SetViewTargetWithBlend(PilotCamera,.25f);}
    else if(ActiveView==3&&ArenaCamera)SetViewTargetWithBlend(ArenaCamera,.3f);
    else if(auto* D=Match->GetSelectedDrone()){D->SetChaseView(true);SetViewTargetWithBlend(D,.25f);}
}
void AImperialPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();
    InputComponent->BindKey(EKeys::One,IE_Pressed,this,&AImperialPlayerController::Fpv);
    InputComponent->BindKey(EKeys::Two,IE_Pressed,this,&AImperialPlayerController::Chase);
    InputComponent->BindKey(EKeys::Three,IE_Pressed,this,&AImperialPlayerController::Arena);
    InputComponent->BindKey(EKeys::Tab,IE_Pressed,this,&AImperialPlayerController::Next);
    InputComponent->BindKey(EKeys::Enter,IE_Pressed,this,&AImperialPlayerController::Auto);
    InputComponent->BindKey(EKeys::H,IE_Pressed,this,&AImperialPlayerController::Hold);
    InputComponent->BindKey(EKeys::R,IE_Pressed,this,&AImperialPlayerController::Return);
    InputComponent->BindKey(EKeys::Home,IE_Pressed,this,&AImperialPlayerController::Reset);
    InputComponent->BindKey(EKeys::F5,IE_Pressed,this,&AImperialPlayerController::Export);
    InputComponent->BindKey(EKeys::C,IE_Pressed,this,&AImperialPlayerController::Radio);
    InputComponent->BindKey(EKeys::LeftMouseButton,IE_Pressed,this,&AImperialPlayerController::ClickHUD);
    InputComponent->BindKey(EKeys::F10,IE_Pressed,this,&AImperialPlayerController::CompactHUD);
    InputComponent->BindKey(EKeys::MouseScrollUp,IE_Pressed,this,&AImperialPlayerController::ZoomIn);
    InputComponent->BindKey(EKeys::MouseScrollDown,IE_Pressed,this,&AImperialPlayerController::ZoomOut);
}
void AImperialPlayerController::ZoomIn(){if(ActiveView==1){if(const auto* H=Cast<AImperialHUD>(GetHUD());H&&H->IsRadioPanelOpen())return;PilotFieldOfView=FMath::Clamp(PilotFieldOfView-5.f,40.f,85.f);UpdatePilotCamera();}}
void AImperialPlayerController::ZoomOut(){if(ActiveView==1){if(const auto* H=Cast<AImperialHUD>(GetHUD());H&&H->IsRadioPanelOpen())return;PilotFieldOfView=FMath::Clamp(PilotFieldOfView+5.f,40.f,85.f);UpdatePilotCamera();}}
bool AImperialPlayerController::HasRadioModal() const {const auto* H=Cast<AImperialHUD>(GetHUD());return H&&H->IsRadioPanelOpen();}
void AImperialPlayerController::Fpv(){if(Match&&!HasRadioModal())Match->ChangeView(1);}
void AImperialPlayerController::Chase(){if(Match&&!HasRadioModal())Match->ChangeView(2);}
void AImperialPlayerController::Arena(){if(Match&&!HasRadioModal())Match->ChangeView(3);}
void AImperialPlayerController::Next(){if(Match&&!HasRadioModal())Match->SelectNext();}
void AImperialPlayerController::Auto(){if(HasRadioModal()){if(auto* H=Cast<AImperialHUD>(GetHUD()))H->AdvanceRadioSetup();return;}if(Match)Match->LaunchSwarm();}
void AImperialPlayerController::Hold(){if(Match&&!HasRadioModal())Match->HoldFleet();}
void AImperialPlayerController::Return(){if(Match&&!HasRadioModal())Match->ReturnFleet();}
void AImperialPlayerController::Reset(){if(Match&&!HasRadioModal())Match->ResetMatch();}
void AImperialPlayerController::Export(){if(Match)Match->ExportTelemetry();}
void AImperialPlayerController::Radio(){if(auto* H=Cast<AImperialHUD>(GetHUD()))H->ToggleRadioPanel();}
void AImperialPlayerController::ClickHUD(){if(auto* H=Cast<AImperialHUD>(GetHUD()))H->ClickPointer();}
void AImperialPlayerController::CompactHUD(){if(auto* H=Cast<AImperialHUD>(GetHUD()))H->ToggleCompactView();}
