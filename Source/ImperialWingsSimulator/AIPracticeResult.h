#pragma once
#include "CoreMinimal.h"
#include "AITraining.h"

struct FImperialAIAircraftResult
{
    int32 Id=0,GuardImpacts=0;
    double GuardRemaining=100;
};
struct FImperialAIResult
{
    FString SessionId,StartedUtc,EndedUtc,StopReason,CompletionReason,SourceHash,BuildVersion,FilePath;
    ImperialAI::Settings Settings;
    double PracticeSeconds=0,LandingSeconds=0;
    int32 Goals=0;
    bool bSnapshot=false,bSaved=false,bTestSession=false;
    TArray<FImperialAIAircraftResult> Aircraft;
    bool Exists() const { return !SessionId.IsEmpty(); }
};
