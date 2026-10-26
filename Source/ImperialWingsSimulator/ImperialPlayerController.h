#pragma once
#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "ImperialPlayerController.generated.h"
class ACameraActor;
class AImperialMatchManager;
UCLASS()
class IMPERIALWINGSSIMULATOR_API AImperialPlayerController : public APlayerController
{
    GENERATED_BODY()
public:
    virtual void BeginPlay() override;
    virtual void SetupInputComponent() override;
    virtual void PlayerTick(float Dt) override;
    void SetView(int32 View);
private:
    UPROPERTY() TObjectPtr<ACameraActor> ArenaCamera;
    UPROPERTY() TObjectPtr<ACameraActor> PilotCamera;
    UPROPERTY() TObjectPtr<AImperialMatchManager> Match;
    bool bPossessed=false;
    int32 ActiveView=1;
    int32 PilotTeam=INDEX_NONE;
    float PilotHeadYaw=0;
    float PilotHeadPitch=0;
    float PilotFieldOfView=85;
    bool bHeadDragging=false;
    FVector2D LastHeadPointer=FVector2D::ZeroVector;
    void ConfigureInput();
    void UpdatePilotCamera();
    bool HasRadioModal() const;
    void ZoomIn(); void ZoomOut();
    void ClickHUD(); void CompactHUD();
    void Fpv(); void Chase(); void Arena(); void Next(); void Auto(); void Hold(); void Return(); void Reset(); void Export(); void Radio();
};
