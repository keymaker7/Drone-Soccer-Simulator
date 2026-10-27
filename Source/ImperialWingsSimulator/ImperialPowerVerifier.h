#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ImperialPowerVerifier.generated.h"
class FJsonValue;
UCLASS()
class IMPERIALWINGSSIMULATOR_API AImperialPowerVerifier : public AActor
{
    GENERATED_BODY()
public:
    AImperialPowerVerifier();
    virtual void Tick(float Dt) override;
private:
    int32 Scenario=-1, Samples=0;
    double Age=0,ModelStart=0,MaxPowerExcess=0,MaxAvailableExcess=0,MaxCurrent=0,MaxThrust=0,MaxAltitude=0;
    double MinDt=1,MaxDt=0;
    bool bLimited=false,bOk=true,bCaptured=false;
    TArray<TSharedPtr<FJsonValue>> Results;
};
