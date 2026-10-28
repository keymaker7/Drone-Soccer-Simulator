#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ImperialAIRecoveryVerifier.generated.h"
class FJsonValue;
class UPhysicsConstraintComponent;
class ASoccerDronePawn;
UCLASS()
class IMPERIALWINGSSIMULATOR_API AImperialAIRecoveryVerifier:public AActor
{
    GENERATED_BODY()
public:
    AImperialAIRecoveryVerifier();
    virtual void Tick(float Dt) override;
private:
    void Check(const TCHAR* Name,bool Passed,double Value=0);
    void Finish();
    void AttachObstruction(ASoccerDronePawn* D);
    void ReleaseObstruction();
    UPROPERTY() TObjectPtr<UPhysicsConstraintComponent> Obstruction;
    TArray<TSharedPtr<FJsonValue>> Checks;
    double Age=0,Total=0,MinDt=1,MaxDt=0,PeakDisplacement=0;
    int32 Stage=0,Case=0,InitialHits=0;
    bool bOk=true,bFinite=true,bPlayerSafe=true;
    FVector InitialPosition;
    FString Capture,PreviousResultId;
};
