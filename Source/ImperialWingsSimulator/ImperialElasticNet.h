#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ImperialElasticNet.generated.h"

class UInstancedStaticMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class USceneComponent;
class UStaticMesh;

/** Visible net response driven by the force model on each aircraft.
 *  Nominal cage planes remain x +/-800, y +/-350, z450 cm. This actor applies no force.
 *  The Gaussian wire deformation and decay are prototype visual approximations.
 */
UCLASS()
class IMPERIALWINGSSIMULATOR_API AImperialElasticNet : public AActor
{
    GENERATED_BODY()
public:
    AImperialElasticNet();
    virtual void Tick(float DeltaSeconds) override;
    // Call before FinishSpawning when using SpawnActorDeferred.
    void Configure(UMaterialInterface* Material, bool bIncludeCeiling);
    // World point in centimetres, world inward-facing normal, physical deflection in metres.
    void SubmitImpact(FVector PointCm, FVector InwardNormal, float DeflectionMeters);
    float GetMaxDeflection() const { return MaximumCurrentDeflectionM; }
    int32 GetWireSegmentCount() const { return Segments.Num(); }
    int32 GetActiveImpactCount() const { return Impacts.Num(); }
protected:
    virtual void BeginPlay() override;
private:
    struct FSegment
    {
        FVector A, B;
        FTransform RestTransform;
        int32 Surface=0, Instance=0;
    };
    struct FImpact
    {
        FVector Point;
        int32 Surface=0;
        float StrengthCm=0, Remaining=0;
    };
    UPROPERTY() TObjectPtr<USceneComponent> NetRoot;
    UPROPERTY() TObjectPtr<UStaticMesh> WireMesh;
    UPROPERTY() TObjectPtr<UMaterialInterface> BaseMaterial;
    UPROPERTY() TArray<TObjectPtr<UInstancedStaticMeshComponent>> WireComponents;
    UPROPERTY() TArray<TObjectPtr<UMaterialInstanceDynamic>> WireMaterials;
    bool bCreateCeiling=true;
    float MaximumCurrentDeflectionM=0;
    TArray<FSegment> Segments;
    TArray<FImpact> Impacts;
    TMap<FIntVector,TArray<int32>> Buckets;
    TSet<int32> PreviouslyDeformed;
    static FTransform WireTransform(FVector A,FVector B);
    static FVector2D SurfacePoint(FVector Point,int32 Surface);
    static FVector OutwardNormal(int32 Surface);
    static float PinnedEdgeFactor(FVector Point,int32 Surface);
    void AddSegment(FVector A,FVector B,int32 Surface);
    void AddWire(FVector A,FVector B,int32 Surface);
    void BuildGrid();
    FVector Displacement(FVector Point,int32 Surface) const;
};
