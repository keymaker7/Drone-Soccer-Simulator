#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ImperialRadioModeVerifier.generated.h"
class FJsonValue;

/** Explicit UI/JSON fixture. It never injects joystick readings or saves a device calibration. */
UCLASS()
class IMPERIALWINGSSIMULATOR_API AImperialRadioModeVerifier : public AActor
{
    GENERATED_BODY()
public:
    AImperialRadioModeVerifier();
    virtual void Tick(float Dt) override;
private:
    int32 Stage=0;
    double Age=0;
    bool bOk=true;
    TArray<TSharedPtr<FJsonValue>> Checks;
    void Check(const TCHAR* Name,bool Passed);
    void CheckJson();
};
