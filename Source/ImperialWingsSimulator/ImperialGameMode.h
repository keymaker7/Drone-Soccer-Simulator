#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "ImperialGameMode.generated.h"
UCLASS()
class IMPERIALWINGSSIMULATOR_API AImperialGameMode : public AGameModeBase
{
    GENERATED_BODY()
public:
    AImperialGameMode();
    virtual void StartPlay() override;
};
