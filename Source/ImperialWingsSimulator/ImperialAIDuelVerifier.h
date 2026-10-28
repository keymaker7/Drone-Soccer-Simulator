#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ImperialAIDuelVerifier.generated.h"
class FJsonValue;
UCLASS()
class AImperialAIDuelVerifier : public AActor
{
    GENERATED_BODY()
public:
    AImperialAIDuelVerifier();
    virtual void Tick(float Dt) override;
private:
    void Check(const TCHAR* Name,bool Passed,double Value=0);
    void Finish();
    int32 Case=0,Stage=0,CaseCount=6,ScoreAtStop=0;
    double Age=0,Total=0,LastSample=-1,MinDt=1e9,MaxDt=0;
    bool bOk=true,bPlayerSafe=true,bRosterSafe=true,bFinite=true,bSawRetreat=false;
    FString Capture;
    TArray<TSharedPtr<FJsonValue>> Checks,Samples;
};
