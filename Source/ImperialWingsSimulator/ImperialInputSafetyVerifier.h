#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ImperialInputSafetyVerifier.generated.h"
class FJsonValue;
/** Explicit engine-key-event fixture, never a physical keyboard or HID capture. */
UCLASS()
class IMPERIALWINGSSIMULATOR_API AImperialInputSafetyVerifier : public AActor
{
    GENERATED_BODY()
public:
    AImperialInputSafetyVerifier();
    virtual void Tick(float Dt) override;
private:
    int32 Stage=0;
    double Age=0;
    bool bOk=true;
    TArray<TSharedPtr<FJsonValue>> Checks;
    void Check(const TCHAR* Name,bool Passed,double Actual=0);
};
