#include "ImperialBuildInfo.h"
#include "ImperialBuildIdentity.generated.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace ImperialBuildInfo
{
FString Version(){return UTF8_TO_TCHAR(IW_BUILD_VERSION);}
FString Commit(){return UTF8_TO_TCHAR(IW_BUILD_COMMIT);}
FString SourceHash(){return UTF8_TO_TCHAR(IW_BUILD_SOURCE_SHA256);}
FString BuiltAt(){return UTF8_TO_TCHAR(IW_BUILD_UTC);}
FString Label(){return FString::Printf(TEXT("v%s  |  %s%s  |  %s UTC"),*Version(),*Commit().Left(8),IW_BUILD_DIRTY?TEXT("*"):TEXT(""),*BuiltAt().Left(16).Replace(TEXT("T"),TEXT(" ")));}
TSharedRef<FJsonObject> Json()
{
    auto Value=MakeShared<FJsonObject>();
    Value->SetStringField(TEXT("version"),Version());
    Value->SetStringField(TEXT("source_commit"),Commit());
    Value->SetBoolField(TEXT("source_dirty"),IW_BUILD_DIRTY!=0);
    Value->SetStringField(TEXT("source_sha256"),SourceHash());
    Value->SetStringField(TEXT("build_utc"),BuiltAt());
    Value->SetStringField(TEXT("target"),UTF8_TO_TCHAR(IW_BUILD_TARGET));
    return Value;
}
void WriteRuntimeIdentity()
{
    FString Value;FJsonSerializer::Serialize(Json(),TJsonWriterFactory<>::Create(&Value));
    FFileHelper::SaveStringToFile(Value,*FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("running-build.json")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    UE_LOG(LogTemp,Display,TEXT("IMPERIAL_BUILD %s source_sha256=%s"),*Label(),*SourceHash());
}
}
