#include "ImperialRadioModeVerifier.h"
#include "ImperialRadioSubsystem.h"
#include "RadioProfileJson.h"
#include "ImperialHUD.h"
#include "ImperialMatchManager.h"
#include "ImperialPlayerController.h"
#include "ImperialBuildInfo.h"
#include "SoccerDronePawn.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"

AImperialRadioModeVerifier::AImperialRadioModeVerifier()
{PrimaryActorTick.bCanEverTick=true;PrimaryActorTick.TickGroup=TG_PostUpdateWork;}
void AImperialRadioModeVerifier::Check(const TCHAR* Name,bool Passed)
{
    auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("check"),Name);Row->SetBoolField(TEXT("ok"),Passed);
    Checks.Add(MakeShared<FJsonValueObject>(Row));bOk&=Passed;
}
void AImperialRadioModeVerifier::CheckJson()
{
    using namespace ImperialRadio;
    for(const auto Mode:{StickMode::Mode1,StickMode::Mode2})
    {
        auto Entry=MakeShared<FJsonObject>();Profile Source;Source.Mode=Mode;WriteProfileMode(Entry,Source);
        FString Json;const bool Written=FJsonSerializer::Serialize(Entry,TJsonWriterFactory<>::Create(&Json));
        TSharedPtr<FJsonObject> Read;Profile Restored;
        const bool Parsed=FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json),Read);
        Check(Mode==StickMode::Mode1?TEXT("Mode 1 JSON round trip"):TEXT("Mode 2 JSON round trip"),
            Written&&Parsed&&ReadProfileMode(Read,Restored)&&Restored.Mode==Mode);
    }
    Profile Legacy;Legacy.Mode=StickMode::Mode1;
    Check(TEXT("legacy profile without stick_mode loads as Mode 2"),ReadProfileMode(MakeShared<FJsonObject>(),Legacy)&&Legacy.Mode==StickMode::Mode2);
    for(const TCHAR* Invalid:{TEXT("{\"stick_mode\":0}"),TEXT("{\"stick_mode\":3}"),TEXT("{\"stick_mode\":1.5}"),TEXT("{\"stick_mode\":\"1\"}"),TEXT("{\"stick_mode\":null}"),TEXT("{\"stick_mode\":true}")})
    {
        TSharedPtr<FJsonObject> Entry;Profile Before;Before.Mode=StickMode::Mode2;
        const bool Parsed=FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(FString(Invalid)),Entry);
        Check(*FString::Printf(TEXT("reject invalid metadata %s without changing output"),Invalid),Parsed&&!ReadProfileMode(Entry,Before)&&Before.Mode==StickMode::Mode2);
    }
}
void AImperialRadioModeVerifier::Tick(float Dt)
{
    Super::Tick(Dt);Age+=Dt;
    auto* PC=Cast<AImperialPlayerController>(UGameplayStatics::GetPlayerController(this,0));
    auto* M=Cast<AImperialMatchManager>(UGameplayStatics::GetActorOfClass(GetWorld(),AImperialMatchManager::StaticClass()));
    auto* H=PC?Cast<AImperialHUD>(PC->GetHUD()):nullptr;
    auto* R=GetGameInstance()->GetSubsystem<UImperialRadioSubsystem>();
    if(!M||!H||!R||!M->GetSelectedDrone())return;
    auto* D=M->GetSelectedDrone();
    FString CaptureDir;FParse::Value(FCommandLine::Get(),TEXT("ImperialRadioModeCapture="),CaptureDir);
    if(Stage==0&&Age>.7)
    {
        M->ResetMatch();CheckJson();
        Check(TEXT("new user defaults to Mode 2 and disabled USB"),R->GetStickMode()==ImperialRadio::StickMode::Mode2&&!R->IsEnabled());
        if(H->IsRadioPanelOpen())H->ToggleRadioPanel();
        Check(TEXT("fixture starts with low throttle ARM"),D->SetArmed(true));H->ToggleRadioPanel();
        Check(TEXT("opening settings disarms selected aircraft"),H->IsRadioPanelOpen()&&!D->IsArmed());
        H->NotifyHitBoxClick(TEXT("Radio_Mode1"));
        Check(TEXT("Mode 1 HUD button selects Mode 1"),R->GetStickMode()==ImperialRadio::StickMode::Mode1);
        Check(TEXT("mode choice cannot enable USB or arm"),!R->IsEnabled()&&!D->IsArmed());
        Check(TEXT("unsupported mode rejected"),!R->SetStickMode(static_cast<ImperialRadio::StickMode>(3))&&R->GetStickMode()==ImperialRadio::StickMode::Mode1);
        Check(TEXT("mode choice does not invent a profile"),!IFileManager::Get().FileExists(*R->GetProfilePath()));
        Stage=1;Age=0;return;
    }
    if(Stage==1&&Age>.7)
    {
        if(!CaptureDir.IsEmpty())FScreenshotRequest::RequestScreenshot(FPaths::Combine(CaptureDir,TEXT("controller-mode1.png")),true,false);
        Stage=2;Age=0;return;
    }
    if(Stage==2&&Age>.7)
    {
        H->ToggleRadioPanel();H->ToggleRadioPanel();
        Check(TEXT("chosen layout survives panel close and reopen"),R->GetStickMode()==ImperialRadio::StickMode::Mode1);
        H->NotifyHitBoxClick(TEXT("Radio_Mode2"));
        Check(TEXT("Mode 2 HUD button selects Mode 2"),R->GetStickMode()==ImperialRadio::StickMode::Mode2);
        Check(TEXT("switching back leaves USB disabled and aircraft disarmed"),!R->IsEnabled()&&!D->IsArmed());
        Stage=3;Age=0;return;
    }
    if(Stage==3&&Age>.7)
    {
        if(!CaptureDir.IsEmpty())FScreenshotRequest::RequestScreenshot(FPaths::Combine(CaptureDir,TEXT("controller-mode2.png")),true,false);
        Stage=4;Age=0;return;
    }
    if(Stage!=4||Age<.7)return;
    Check(TEXT("UI test never writes a calibration file"),!IFileManager::Get().FileExists(*R->GetProfilePath()));
    auto Result=MakeShared<FJsonObject>();Result->SetBoolField(TEXT("ok"),bOk);Result->SetArrayField(TEXT("checks"),Checks);
    Result->SetObjectField(TEXT("build"),ImperialBuildInfo::Json());
    Result->SetStringField(TEXT("scope"),TEXT("explicit HUD button APIs and actual JSON mode codec in isolated user data; no device selected, no HID injection, no physical-controller verification"));
    FString Json,File;FJsonSerializer::Serialize(Result,TJsonWriterFactory<>::Create(&Json));FParse::Value(FCommandLine::Get(),TEXT("ImperialRadioModeResult="),File);
    FFileHelper::SaveStringToFile(Json,*File,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    FPlatformMisc::RequestExitWithStatus(false,bOk?0:1);SetActorTickEnabled(false);
}
