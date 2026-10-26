#include "ImperialGameMode.h"
#include "ImperialPlayerController.h"
#include "ImperialMatchManager.h"
#include "ImperialHUD.h"
#include "PrototypeArenaBuilder.h"
AImperialGameMode::AImperialGameMode()
{
    DefaultPawnClass=nullptr; PlayerControllerClass=AImperialPlayerController::StaticClass(); HUDClass=AImperialHUD::StaticClass();
}
void AImperialGameMode::StartPlay()
{
    GetWorld()->SpawnActor<APrototypeArenaBuilder>();
    GetWorld()->SpawnActor<AImperialMatchManager>();
    Super::StartPlay();
}
