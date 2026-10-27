#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ImperialTimeVerifier.generated.h"
class FJsonValue;
/** Bounded real-time Chaos clock audit. No physical keyboard, HID or remote bridge. */
UCLASS()
class IMPERIALWINGSSIMULATOR_API AImperialTimeVerifier : public AActor
{
    GENERATED_BODY()
public:
    AImperialTimeVerifier();
    virtual void BeginPlay() override;
    virtual void Tick(float Dt) override;
private:
    double Age=0,GameStart=0,SolverStart=0,LastSolver=0,WallStart=0,GameTotal=0,PhysicsTotal=0;
    double LedgerGameStart=0,LedgerPhysicsStart=0,LedgerDroppedStart=0;
    double ModelStart[10]={},SensorStart[10]={},BatteryPrevious[10]={},BatteryError[10]={};
    double LargestModelError=0,LargestSensorError=0,MaximumTilt=0,MaximumSpeed=0,MaximumHoldError=0;
    FVector Targets[10];
    bool bLaunched=false,bMeasuring=false,bHitches=false,bInjected100=false,bInjected250=false;
    bool bOfficial=false,bMatchSafe=true;
    bool bCaptureRequested=false;
    TArray<TSharedPtr<FJsonValue>> Frames;
};
