#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ImperialRoleRibbons.generated.h"

class ASoccerDronePawn;
class UInstancedStaticMeshComponent;
class UStaticMesh;
class UMaterialInterface;
class UMaterialInstanceDynamic;

/** Lower role markers: two ribbons per side, four per Striker/Guide aircraft. */
UCLASS()
class IMPERIALWINGSSIMULATOR_API AImperialRoleRibbons : public AActor
{
    GENERATED_BODY()
public:
    AImperialRoleRibbons();
    virtual void Tick(float DeltaSeconds) override;
protected:
    virtual void BeginPlay() override;
private:
    struct FRibbonSet
    {
        TWeakObjectPtr<ASoccerDronePawn> Drone;
        TWeakObjectPtr<UInstancedStaticMeshComponent> Mesh;
        TWeakObjectPtr<UMaterialInstanceDynamic> Material;
        int32 Role = INDEX_NONE;
    };
    UPROPERTY() TObjectPtr<UStaticMesh> StripMesh;
    UPROPERTY() TObjectPtr<UMaterialInterface> StripMaterial;
    TArray<FRibbonSet> Aircraft;
    double Elapsed = 0;
    float DiscoveryClock = 1;
    bool bVerified = false;
    void Discover();
    void Build(FRibbonSet& Set);
    void VerifyLayout();
};
