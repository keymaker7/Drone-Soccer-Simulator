#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ImperialSuspendedGoal.generated.h"
class USphereComponent;
class UStaticMeshComponent;
class UInstancedStaticMeshComponent;
class UPhysicsConstraintComponent;
class UMaterialInterface;
class UPhysicalMaterial;

/** Force-driven compound hoop suspended by a damped pendulum constraint.
 *  Mass/damping/limits are prototype values, not measured arena hardware.
 */
UCLASS()
class IMPERIALWINGSSIMULATOR_API AImperialSuspendedGoal : public AActor
{
    GENERATED_BODY()
public:
    AImperialSuspendedGoal();
    virtual void BeginPlay() override;
    virtual void Tick(float Dt) override;
    void Configure(int32 Index,UMaterialInterface* Frame,UMaterialInterface* Signal);
    FVector GetCentre() const;
    FQuat GetRingRotation() const;
    float GetSwayCm() const;
    float GetPeakSwayCm() const { return PeakSway; }
    int32 GetImpactCount() const { return ImpactCount; }
    void ResetGoal();
    void SetCompetitionStable(bool bStable);
    USphereComponent* GetPhysicsRoot() const { return PhysicsRoot; }
    bool HasOpenPhysicalHoop() const;
private:
    UPROPERTY() TObjectPtr<USphereComponent> PhysicsRoot;
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Segments;
    UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> ReadyLights;
    UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Cables;
    UPROPERTY() TObjectPtr<UPhysicsConstraintComponent> Suspension;
    UPROPERTY() TObjectPtr<UPhysicalMaterial> ContactMaterial;
    FVector RestTop,RestCentre;
    FQuat RestRotation=FQuat::Identity;
    int32 GoalIndex=0,ImpactCount=0;
    float PeakSway=0;
    double LastImpactTime=-100;
    UFUNCTION() void OnGoalHit(UPrimitiveComponent* Component,AActor* Other,UPrimitiveComponent* OtherComponent,FVector Impulse,const FHitResult& Hit);
};
