#include "ImperialInputSafetyVerifier.h"
#include "ImperialMatchManager.h"
#include "ImperialPlayerController.h"
#include "ImperialHUD.h"
#include "ImperialRadioSubsystem.h"
#include "ImperialBuildInfo.h"
#include "SoccerDronePawn.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/GameViewportClient.h"
#include "Engine/GameInstance.h"
#include "InputKeyEventArgs.h"
#include "InputCoreTypes.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"

AImperialInputSafetyVerifier::AImperialInputSafetyVerifier()
{PrimaryActorTick.bCanEverTick=true;PrimaryActorTick.TickGroup=TG_PostUpdateWork;}
void AImperialInputSafetyVerifier::Check(const TCHAR* Name,bool Passed,double Actual)
{
    auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("check"),Name);Row->SetBoolField(TEXT("ok"),Passed);Row->SetNumberField(TEXT("actual"),Actual);
    Checks.Add(MakeShared<FJsonValueObject>(Row));bOk&=Passed;
}
void AImperialInputSafetyVerifier::Tick(float Dt)
{
    Super::Tick(Dt);Age+=Dt;
    auto* M=Cast<AImperialMatchManager>(UGameplayStatics::GetActorOfClass(GetWorld(),AImperialMatchManager::StaticClass()));
    auto* PC=Cast<AImperialPlayerController>(UGameplayStatics::GetPlayerController(this,0));
    auto* D=M?M->GetSelectedDrone():nullptr;auto* V=GetWorld()->GetGameViewport();auto* H=PC?Cast<AImperialHUD>(PC->GetHUD()):nullptr;
    if(!PC||!D||!V||!H)return;
    const auto Key=[&](FKey K,EInputEvent Event){PC->InputKey(FInputKeyEventArgs::CreateSimulated(K,Event,Event==IE_Released?0:1));};
    const auto Axes=[&](EInputEvent Event){for(const FKey K:{EKeys::Up,EKeys::D,EKeys::W,EKeys::E})Key(K,Event);};
    if(Stage==0&&Age>.6)
    {
        M->ResetMatch();M->ChangeView(1);if(H->IsRadioPanelOpen())H->ToggleRadioPanel();
        Check(TEXT("fixture starts with a manual keyboard aircraft"),!D->IsAutonomousEnabled()&&D->GetInputSource()==TEXT("KEYBOARD"));
        Check(TEXT("explicit low-throttle ARM accepted"),D->SetArmed(true));Axes(IE_Pressed);Stage=1;Age=0;return;
    }
    if(Stage==1&&Age>.4)
    {
        Check(TEXT("real engine bindings accumulated keyboard throttle"),D->IsArmed()&&D->GetThrottle()>.08,D->GetThrottle());
        Check(TEXT("engine saw held roll key"),PC->IsInputKeyDown(EKeys::D));
        auto Queued=FInputKeyEventArgs::CreateSimulated(EKeys::Down,IE_Pressed,1);Queued.EventTimestamp=FPlatformTime::Cycles64();
        V->LostFocus(V->Viewport);
        Check(TEXT("viewport focus loss immediately disarms manual aircraft"),!D->IsArmed());
        Check(TEXT("viewport focus loss immediately clears stored throttle"),D->GetThrottle()<.0001,D->GetThrottle());
        Check(TEXT("viewport focus loss immediately clears motor demand"),D->GetTotalThrustNewtons()<.0001,D->GetTotalThrustNewtons());
        auto* Radio=GetGameInstance()->GetSubsystem<UImperialRadioSubsystem>();Check(TEXT("native radio remains disabled"),Radio&&!Radio->IsEnabled());
        PC->InputKey(Queued);
        Axes(IE_Repeat);Key(EKeys::SpaceBar,IE_Repeat);Stage=2;Age=0;return;
    }
    if(Stage==2&&Age>.3)
    {
        Check(TEXT("old repeated keys cannot restore throttle"),D->GetThrottle()<.0001,D->GetThrottle());
        Check(TEXT("old repeated roll key stays released in engine"),!PC->IsInputKeyDown(EKeys::D));
        Check(TEXT("queued pre-interruption key press is rejected"),!PC->IsInputKeyDown(EKeys::Down));
        Check(TEXT("GameAndUI always requests focus-loss key flushing"),PC->ShouldFlushKeysWhenViewportFocusChanges());
        Check(TEXT("repeat cannot re-arm after focus loss"),!D->IsArmed());V->ReceivedFocus(V->Viewport);
        Check(TEXT("focus regain does not re-arm"),!D->IsArmed());
        Axes(IE_Released);Key(EKeys::SpaceBar,IE_Released);D->ResetDrone();PC->FlushPressedKeys();
        Key(EKeys::SpaceBar,IE_Pressed);Stage=3;Age=0;return;
    }
    if(Stage==3&&Age>.15)
    {
        Check(TEXT("fresh Space press re-arms at zero throttle"),D->IsArmed()&&D->GetThrottle()<.0001);
        Key(EKeys::SpaceBar,IE_Released);M->ChangeView(2);Check(TEXT("observer view preserves manual ARM"),D->IsArmed());
        M->ChangeView(3);Check(TEXT("overview preserves manual ARM"),D->IsArmed());M->ChangeView(1);
        H->ToggleRadioPanel();Check(TEXT("opening radio menu disarms selected aircraft"),H->IsRadioPanelOpen()&&!D->IsArmed());
        Axes(IE_Pressed);Key(EKeys::SpaceBar,IE_Pressed);Stage=4;Age=0;return;
    }
    if(Stage==4&&Age>.3)
    {
        Check(TEXT("menu typing cannot arm or add throttle"),!D->IsArmed()&&D->GetThrottle()<.0001);
        H->ToggleRadioPanel();Axes(IE_Repeat);Key(EKeys::SpaceBar,IE_Repeat);Stage=5;Age=0;return;
    }
    if(Stage==5&&Age>.3)
    {
        Check(TEXT("closing menu rejects old repeated throttle/ARM"),!D->IsArmed()&&D->GetThrottle()<.0001,D->GetThrottle());
        Axes(IE_Released);Key(EKeys::SpaceBar,IE_Released);PC->FlushPressedKeys();
        D->ResetDrone();D->SetAutonomousEnabled(true);D->SetAutonomousTarget(D->GetActorLocation()+FVector(0,0,80));
        V->LostFocus(V->Viewport);Check(TEXT("focus loss preserves explicit practice autonomy"),D->IsAutonomousEnabled()&&D->IsArmed());
        Stage=6;Age=0;return;
    }
    if(Stage==6&&Age>.3)
    {
        Check(TEXT("autonomous forces still run after focus loss"),D->IsArmed()&&D->GetTotalThrustNewtons()>0,D->GetTotalThrustNewtons());
        D->SetAutonomousEnabled(false);D->ResetDrone();Axes(IE_Pressed);Stage=7;Age=0;return;
    }
    if(Stage==7&&Age>.3)
    {
        Check(TEXT("second throttle setup reached engine bindings"),D->GetThrottle()>.05,D->GetThrottle());
        M->SelectNext();Stage=8;Age=0;return;
    }
    if(Stage==8&&Age>.3)
    {
        Check(TEXT("aircraft selection clears held-input throttle on new aircraft"),!D->IsArmed()&&D->GetThrottle()<.0001,D->GetThrottle());
        PC->ToggleEquipmentInspection();Check(TEXT("equipment inspection opened disarmed"),PC->IsEquipmentInspection()&&!D->IsArmed());
        Axes(IE_Pressed);Key(EKeys::SpaceBar,IE_Pressed);Stage=9;Age=0;return;
    }
    if(Stage==9&&Age>.3)
    {
        Check(TEXT("equipment view blocks held flight keys"),!D->IsArmed()&&D->GetThrottle()<.0001,D->GetThrottle());
        PC->ToggleEquipmentInspection();Axes(IE_Repeat);Key(EKeys::SpaceBar,IE_Repeat);Stage=10;Age=0;return;
    }
    if(Stage==10&&Age>.3)
    {
        Check(TEXT("equipment exit rejects stale throttle/ARM repeats"),!PC->IsEquipmentInspection()&&!D->IsArmed()&&D->GetThrottle()<.0001,D->GetThrottle());
        Axes(IE_Released);Key(EKeys::SpaceBar,IE_Released);Key(EKeys::Up,IE_Pressed);Stage=11;Age=0;return;
    }
    if(Stage==11&&Age>.3)
    {
        Check(TEXT("reset setup accumulated keyboard throttle"),D->GetThrottle()>.05,D->GetThrottle());
        Key(EKeys::BackSpace,IE_Pressed);Key(EKeys::SpaceBar,IE_Pressed);Stage=12;Age=0;return;
    }
    if(Stage==12&&Age>.2)
    {
        Key(EKeys::BackSpace,IE_Released);Key(EKeys::SpaceBar,IE_Released);Key(EKeys::Up,IE_Repeat);Stage=13;Age=0;return;
    }
    if(Stage!=13||Age<.3)return;
    Check(TEXT("Backspace reset clears pending throttle repeats"),!D->IsArmed()&&D->GetThrottle()<.0001,D->GetThrottle());
    M->ResetMatch();auto Result=MakeShared<FJsonObject>();Result->SetBoolField(TEXT("ok"),bOk);Result->SetArrayField(TEXT("checks"),Checks);
    Result->SetStringField(TEXT("scope"),TEXT("explicit Unreal simulated keyboard events and real viewport/controller APIs; not physical keyboard, OS Alt-Tab or USB/HID input"));
    Result->SetObjectField(TEXT("build"),ImperialBuildInfo::Json());FString Text;FJsonSerializer::Serialize(Result,TJsonWriterFactory<>::Create(&Text));
    FString File;FParse::Value(FCommandLine::Get(),TEXT("ImperialInputResult="),File);FFileHelper::SaveStringToFile(Text,*File);
    FPlatformMisc::RequestExitWithStatus(false,bOk?0:1);SetActorTickEnabled(false);
}
