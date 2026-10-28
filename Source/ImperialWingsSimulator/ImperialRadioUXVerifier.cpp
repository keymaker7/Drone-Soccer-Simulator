#include "ImperialRadioUXVerifier.h"
#include "ImperialLocalization.h"
#include "ImperialProfileStorage.h"
#include "ImperialUserData.h"
#include "ImperialRadioSubsystem.h"
#include "ImperialHUD.h"
#include "ImperialMatchManager.h"
#include "ImperialPlayerController.h"
#include "ImperialBuildInfo.h"
#include "SoccerDronePawn.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#endif

namespace
{
bool ContainsHangul(const FString& Text)
{
    for(const TCHAR C:Text)
        if((C>=0xAC00&&C<=0xD7A3)||(C>=0x1100&&C<=0x11FF)||(C>=0x3130&&C<=0x318F))return true;
    return false;
}
bool ReadEquals(const FString& File,const FString& Expected)
{
    FString Actual;
    return FFileHelper::LoadFileToString(Actual,*File)&&Actual.TrimStartAndEnd()==Expected;
}
bool AllDisarmed(AImperialMatchManager& Match)
{
    if(Match.GetDrones().Num()!=10)return false;
    for(const auto& Drone:Match.GetDrones())if(!Drone||Drone->IsArmed())return false;
    return true;
}
}
AImperialRadioUXVerifier::AImperialRadioUXVerifier()
{PrimaryActorTick.bCanEverTick=true;PrimaryActorTick.TickGroup=TG_PostUpdateWork;}
void AImperialRadioUXVerifier::Check(const TCHAR* Name,bool Passed)
{
    auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("check"),Name);Row->SetBoolField(TEXT("ok"),Passed);
    Checks.Add(MakeShared<FJsonValueObject>(Row));bOk&=Passed;
}
void AImperialRadioUXVerifier::CheckStorage()
{
    const FString Folder=ImperialUserData::Path(TEXT("Verification/ProfileStorage"));
    const FString Target=FPaths::Combine(Folder,TEXT("storage-test.json"));
    Check(TEXT("atomic storage creates a new isolated QA file"),ImperialProfileStorage::Save(Target,TEXT("original"))&&ReadEquals(Target,TEXT("original")));
    Check(TEXT("atomic storage replaces an existing isolated QA file"),ImperialProfileStorage::Save(Target,TEXT("replacement"))&&ReadEquals(Target,TEXT("replacement")));
    Check(TEXT("empty destination is rejected without touching existing contents"),!ImperialProfileStorage::Save(FString(),TEXT("invalid"))&&ReadEquals(Target,TEXT("replacement")));
    const FString UnicodePath=FPaths::Combine(Folder,TEXT("한글-저장-검사.json"));
    Check(TEXT("Unicode path and UTF-8 Korean contents round trip"),ImperialProfileStorage::Save(UnicodePath,TEXT("한국어 저장 검사"))&&ReadEquals(UnicodePath,TEXT("한국어 저장 검사")));
#if PLATFORM_WINDOWS
    // Denying delete sharing exercises the real Windows rename/replace failure.
    // This QA file is unrelated to RadioProfile.json and contains no calibration.
    HANDLE Lock=CreateFileW(*Target,GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    Check(TEXT("destination lock fixture opened"),Lock!=INVALID_HANDLE_VALUE);
    bool Rejected=false;
    if(Lock!=INVALID_HANDLE_VALUE)
    {
        Rejected=!ImperialProfileStorage::Save(Target,TEXT("must never replace locked destination"));
        CloseHandle(Lock);
    }
    Check(TEXT("locked destination rejects replacement and preserves its previous contents"),Rejected&&ReadEquals(Target,TEXT("replacement")));
#endif
    const FString Blocker=FPaths::Combine(Folder,TEXT("parent-is-a-file"));
    const bool Blocked=FFileHelper::SaveStringToFile(TEXT("retain parent"),*Blocker,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    Check(TEXT("non-directory parent rejects storage and retains parent file"),Blocked&&!ImperialProfileStorage::Save(FPaths::Combine(Blocker,TEXT("child.json")),TEXT("not writable"))&&ReadEquals(Blocker,TEXT("retain parent")));
    TArray<FString> Files;
    IFileManager::Get().FindFiles(Files,*FPaths::Combine(Folder,TEXT("*")),true,false);
    bool NoTemps=true;
    for(const auto& File:Files)if(File.Contains(TEXT(".tmp-")))NoTemps=false;
    Check(TEXT("successful and failed writes leave no temporary siblings"),NoTemps&&Files.Num()==3);
}
void AImperialRadioUXVerifier::Finish()
{
    auto Result=MakeShared<FJsonObject>();Result->SetBoolField(TEXT("ok"),bOk);Result->SetArrayField(TEXT("checks"),Checks);
    Result->SetObjectField(TEXT("build"),ImperialBuildInfo::Json());
    Result->SetStringField(TEXT("scope"),TEXT("isolated new user data, explicit HUD language/layout button APIs and real Windows file-replacement failures; no device selection, HID injection, physical USB flight, cable-loss or other-PC verification"));
    FString Json,File;FJsonSerializer::Serialize(Result,TJsonWriterFactory<>::Create(&Json));
    FParse::Value(FCommandLine::Get(),TEXT("ImperialRadioUXResult="),File);
    const bool Saved=!File.IsEmpty()&&FFileHelper::SaveStringToFile(Json,*File,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    FPlatformMisc::RequestExitWithStatus(false,bOk&&Saved?0:1);SetActorTickEnabled(false);
}
void AImperialRadioUXVerifier::Tick(float Dt)
{
    Super::Tick(Dt);Age+=Dt;
    auto* PC=Cast<AImperialPlayerController>(UGameplayStatics::GetPlayerController(this,0));
    auto* M=Cast<AImperialMatchManager>(UGameplayStatics::GetActorOfClass(GetWorld(),AImperialMatchManager::StaticClass()));
    auto* H=PC?Cast<AImperialHUD>(PC->GetHUD()):nullptr;
    auto* R=GetGameInstance()->GetSubsystem<UImperialRadioSubsystem>();
    if(!M||!H||!R||!M->GetSelectedDrone())return;
    FString CaptureDir;FParse::Value(FCommandLine::Get(),TEXT("ImperialRadioUXCapture="),CaptureDir);
    const FString LanguagePath=ImperialUserData::Path(TEXT("Config/Language.txt"));
    if(Stage==0&&Age>.75)
    {
        FString DataDir;
        const bool Isolated=FParse::Value(FCommandLine::Get(),TEXT("ImperialDataDir="),DataDir)&&!DataDir.IsEmpty()&&!FPaths::IsRelative(DataDir)&&FPaths::IsSamePath(DataDir,ImperialUserData::Directory());
        Check(TEXT("explicit absolute isolated user data is required"),Isolated);
        if(!Isolated){Finish();return;}
        Check(TEXT("no device is selected and no USB input enabled"),!R->GetSelectedDevice()&&!R->IsEnabled());
        Check(TEXT("no real or synthetic calibration is present"),!IFileManager::Get().FileExists(*R->GetProfilePath()));
        if(FParse::Param(FCommandLine::Get(),TEXT("ImperialRadioUXRestoreOnly")))
        {
            FString Expected;FParse::Value(FCommandLine::Get(),TEXT("ImperialRadioUXExpectedLanguage="),Expected);
            Check(TEXT("fresh process restores expected language or explicit CLI override"),ImperialLocalization::IsKorean()==(Expected==TEXT("ko")));
            Check(TEXT("restoring or overriding display leaves persisted English selection unchanged"),ReadEquals(LanguagePath,TEXT("en")));
            Check(TEXT("language restoration leaves all aircraft disarmed"),AllDisarmed(*M));
            Finish();return;
        }
        Check(TEXT("new user starts in Korean without a prewritten language setting"),ImperialLocalization::IsKorean()&&!IFileManager::Get().FileExists(*LanguagePath));
        Check(TEXT("new user retains default Mode 2"),R->GetStickMode()==ImperialRadio::StickMode::Mode2);
        if(!H->IsRadioPanelOpen())H->ToggleRadioPanel();
        Check(TEXT("opening controller panel leaves all ten aircraft disarmed"),H->IsRadioPanelOpen()&&AllDisarmed(*M));
        for(const TCHAR* Label:{TEXT("ENABLE RADIO"),TEXT("RADIO / USB CONTROLLER"),TEXT("THROTTLE"),TEXT("YAW")})
            Check(*FString::Printf(TEXT("Korean controller label translates: %s"),Label),ContainsHangul(ImperialLocalization::Translate(Label)));
        Check(TEXT("formatted leading count translates and preserves its value"),ImperialLocalization::Translate(TEXT("1 device(s) found. Choose your radio below, or use NEXT DEVICE."))==TEXT("장치 1개를 찾았습니다. 사용할 조종기를 아래에서 선택하세요."));
        Check(TEXT("formatted saved mode message translates and preserves Mode 2"),ContainsHangul(ImperialLocalization::Translate(TEXT("Mode 2 device calibration loaded. Keyboard active; minimum throttle and explicit ENABLE USB required.")))&&ImperialLocalization::Translate(TEXT("Mode 2 device calibration loaded. Keyboard active; minimum throttle and explicit ENABLE USB required.")).Contains(TEXT("2")));
        Check(TEXT("physical-controller ARM failure is translated"),ContainsHangul(ImperialLocalization::Translate(TEXT("USB INPUT LOST / DISARMED"))));
        Check(TEXT("unrecognized device name remains unchanged"),ImperialLocalization::Translate(TEXT("USER USB DEVICE 123"))==TEXT("USER USB DEVICE 123"));
        R->DisableRadio();
        Check(TEXT("controller status is translated in Korean"),ContainsHangul(R->GetStatus()));
        H->NotifyHitBoxClick(TEXT("Radio_Mode1"));
        Check(TEXT("Korean UI preserves explicit Mode 1 selection without enabling USB"),R->GetStickMode()==ImperialRadio::StickMode::Mode1&&!R->IsEnabled()&&AllDisarmed(*M));
        CheckStorage();Stage=1;Age=0;return;
    }
    if(Stage==1&&Age>.75)
    {
        if(!CaptureDir.IsEmpty())FScreenshotRequest::RequestScreenshot(FPaths::Combine(CaptureDir,TEXT("controller-ko.png")),true,false);
        Stage=2;Age=0;return;
    }
    if(Stage==2&&Age>.75)
    {
        H->NotifyHitBoxClick(TEXT("Radio_Language"));
        Check(TEXT("language button switches to English and saves only language preference"),!ImperialLocalization::IsKorean()&&ReadEquals(LanguagePath,TEXT("en")));
        Check(TEXT("English controller label is unchanged"),ImperialLocalization::Translate(TEXT("ENABLE RADIO"))==TEXT("ENABLE RADIO"));
        R->DisableRadio();
        Check(TEXT("existing status renders English after changing language"),!ContainsHangul(R->GetStatus())&&R->GetStatus().Contains(TEXT("Keyboard input selected")));
#if PLATFORM_WINDOWS
        HANDLE LanguageLock=CreateFileW(*LanguagePath,GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
        Check(TEXT("language preference lock fixture opened"),LanguageLock!=INVALID_HANDLE_VALUE);
        bool LanguageRejected=false;
        if(LanguageLock!=INVALID_HANDLE_VALUE)
        {
            LanguageRejected=!ImperialLocalization::SetKorean(true);
            CloseHandle(LanguageLock);
        }
        Check(TEXT("failed language save retains both active English and persisted English"),LanguageRejected&&!ImperialLocalization::IsKorean()&&ReadEquals(LanguagePath,TEXT("en")));
        TArray<FString> LanguageTemps;
        IFileManager::Get().FindFiles(LanguageTemps,*FPaths::Combine(FPaths::GetPath(LanguagePath),TEXT("Language.txt.tmp*")),true,false);
        Check(TEXT("failed language save leaves no temporary preference file"),LanguageTemps.IsEmpty());
#endif
        Check(TEXT("language switch preserves chosen layout and motor/input gates"),R->GetStickMode()==ImperialRadio::StickMode::Mode1&&!R->IsEnabled()&&AllDisarmed(*M));
        Stage=3;Age=0;return;
    }
    if(Stage==3&&Age>.75)
    {
        if(!CaptureDir.IsEmpty())FScreenshotRequest::RequestScreenshot(FPaths::Combine(CaptureDir,TEXT("controller-en.png")),true,false);
        Stage=4;Age=0;return;
    }
    if(Stage!=4||Age<.75)return;
    H->NotifyHitBoxClick(TEXT("Radio_Language"));
    Check(TEXT("switching back to Korean persists ko"),ImperialLocalization::IsKorean()&&ReadEquals(LanguagePath,TEXT("ko")));
    H->NotifyHitBoxClick(TEXT("Radio_Mode2"));
    Check(TEXT("Korean UI preserves Mode 2 selection without changing motor/input gates"),R->GetStickMode()==ImperialRadio::StickMode::Mode2&&!R->IsEnabled()&&AllDisarmed(*M));
    H->NotifyHitBoxClick(TEXT("Radio_Language"));
    Check(TEXT("fixture leaves English persisted for independent process reload"),!ImperialLocalization::IsKorean()&&ReadEquals(LanguagePath,TEXT("en")));
    Check(TEXT("UI and storage tests never create a calibration file"),!IFileManager::Get().FileExists(*R->GetProfilePath()));
    Finish();
}
