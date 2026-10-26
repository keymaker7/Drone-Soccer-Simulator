#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PrototypeArenaBuilder.generated.h"

class USceneComponent;
class UStaticMesh;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UInstancedStaticMeshComponent;
class UPhysicalMaterial;
class AImperialElasticNet;

/** Creates a FIDA 2026 Class 40 recommended 16 x 7 x 4.5 m court at BeginPlay.
 *  Elastic-net response coefficients and 16 cm goal depth are prototype choices.
 */
UCLASS()
class IMPERIALWINGSSIMULATOR_API APrototypeArenaBuilder : public AActor
{
    GENERATED_BODY()

public:
    APrototypeArenaBuilder();
    void SetGoalSignal(int32 GoalIndex, bool bScored);
    bool GetStartPadPosition(int32 AircraftId,FVector& Position) const;

protected:
    virtual void BeginPlay() override;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<USceneComponent> ArenaRoot;

    UPROPERTY()
    TObjectPtr<UStaticMesh> CubeMesh;

    UPROPERTY(EditAnywhere, Category="Arena")
    bool bCreateCeiling = true;

    UPROPERTY(EditAnywhere, Category="Arena")
    bool bCreateLights = true;

private:
    UPROPERTY() TObjectPtr<UStaticMesh> CylinderMesh;
    UPROPERTY() TObjectPtr<UStaticMesh> SphereMesh;
    UPROPERTY() TObjectPtr<UMaterialInterface> SurfaceMaterial;
    UPROPERTY() TObjectPtr<UMaterialInterface> NetMaterial;
    UPROPERTY() TObjectPtr<UMaterialInterface> CourtMaterial;
    UPROPERTY() TObjectPtr<UMaterialInterface> ConcreteMaterial;
    UPROPERTY() TObjectPtr<UMaterialInterface> MetalMaterial;
    UPROPERTY() TObjectPtr<UMaterialInterface> RubberMaterial;
    UPROPERTY() TObjectPtr<UPhysicalMaterial> FloorPhysicalMaterial;
    UPROPERTY() TObjectPtr<UPhysicalMaterial> GoalPhysicalMaterial;
    UPROPERTY() TObjectPtr<UPhysicalMaterial> BackupPhysicalMaterial;
    UPROPERTY() TObjectPtr<AImperialElasticNet> ElasticNet;
    UPROPERTY() TArray<TObjectPtr<UMaterialInstanceDynamic>> Materials;
    UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> StructureMaterial;
    UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> BlueMaterial;
    UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> RedMaterial;
    UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> GoalFrameMaterial;
    UPROPERTY() TArray<TObjectPtr<UMaterialInstanceDynamic>> GoalLights;
    bool GoalIsScored[2] = {false, false};
    TMap<FString, UInstancedStaticMeshComponent*> Batches;
    TArray<TWeakObjectPtr<UInstancedStaticMeshComponent>> StartPadBatches;
    TArray<int32> StartPadIndices;
    UMaterialInstanceDynamic* Material(FLinearColor Color, float Glow = 0.f, UMaterialInterface* BaseOverride = nullptr);
    void SteelBeam(const FString& Name, FVector Start, FVector End, float Depth = 24.f, float Width = 14.f);
    void Piece(const FString& Name, UStaticMesh* Mesh, UMaterialInterface* Mat,
        FVector PositionCm, FVector SizeCm, FRotator Rotation = FRotator::ZeroRotator,
        bool bCollision = false, bool bInvisible = false);
    void Label(const FString& Name, const FString& Value, FVector PositionCm,
        FRotator Rotation, float Size, FColor Color);
    void AddBox(const FString& Name, const FVector& LocationCm,
        const FVector& SizeCm, bool bCollision);
    void AddGoal(const FString& Prefix, float X);
    void AddLight(const FString& Name, const FVector& LocationCm);
};
