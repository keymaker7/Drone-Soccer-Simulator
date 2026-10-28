#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ImperialAIPracticeVerifier.generated.h"
class FJsonValue;
UCLASS()
class IMPERIALWINGSSIMULATOR_API AImperialAIPracticeVerifier:public AActor
{
    GENERATED_BODY()
public:
    AImperialAIPracticeVerifier();
    virtual void Tick(float Dt) override;
private:
    void Check(const TCHAR* Name,bool Passed,double Value=0);
    void Finish();
    double Age=0,Total=0,MinDt=1,MaxDt=0,MaxSpeed=0,MaxHeight=0,MaxError=0,MinAirborne=100;
    int32 Stage=0,Case=0;
    bool bOk=true,bFinite=true,bCountSafe=true,bCaptured=false;
    FString Capture;
    TArray<TSharedPtr<FJsonValue>> Checks;
};
