#include "ImperialUserData.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"

FString ImperialUserData::Directory()
{
    static const FString Root=[]()
    {
        FString Override;
        // Explicit isolated QA data. Never read a developer profile as fallback.
        if(FParse::Value(FCommandLine::Get(),TEXT("ImperialDataDir="),Override)&&!Override.IsEmpty()&&!FPaths::IsRelative(Override))
            return FPaths::ConvertRelativePathToFull(Override);
#if WITH_EDITOR
        return FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir());
#else
        return FPaths::Combine(FPlatformProcess::UserSettingsDir(),TEXT("ImperialWingsSimulator/Saved"));
#endif
    }();
    IFileManager::Get().MakeDirectory(*Root,true);
    return Root;
}
FString ImperialUserData::Path(const FString& Relative){return FPaths::Combine(Directory(),Relative);}
