#pragma once
#include "CoreMinimal.h"
class FJsonObject;
namespace ImperialBuildInfo
{
    FString Version();
    FString Commit();
    FString SourceHash();
    FString BuiltAt();
    FString Label();
    TSharedRef<FJsonObject> Json();
    void WriteRuntimeIdentity();
}
