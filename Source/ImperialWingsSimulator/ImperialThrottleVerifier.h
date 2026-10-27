#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ImperialThrottleVerifier.generated.h"
class FJsonValue;
UCLASS()
class IMPERIALWINGSSIMULATOR_API AImperialThrottleVerifier : public AActor
{
    GENERATED_BODY()
public:
    AImperialThrottleVerifier();
    virtual void Tick(float Dt) override;
private:
    int32 Scenario=-1;
    double Age=0,ModelStart=0;
    double MinDt=1,MaxDt=0;
    bool bOk=true;
    double FinalThrust[6]={};
    TArray<TSharedPtr<FJsonValue>> Checks;
    void Check(const TCHAR* Name,bool Passed,double Value=0);
    void Finish();
};
