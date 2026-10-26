#include "ImperialGameMode.h"
#include "ImperialPlayerController.h"
#include "ImperialMatchManager.h"
#include "ImperialHUD.h"
#include "PrototypeArenaBuilder.h"
#include "ImperialPresentationEffects.h"
#include "ImperialRoleRibbons.h"
#include "ImperialInputSafetyVerifier.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
AImperialGameMode::AImperialGameMode()
{
    DefaultPawnClass=nullptr; PlayerControllerClass=AImperialPlayerController::StaticClass(); HUDClass=AImperialHUD::StaticClass();
}
void AImperialGameMode::StartPlay()
{
    GetWorld()->SpawnActor<APrototypeArenaBuilder>();
    GetWorld()->SpawnActor<AImperialMatchManager>();
    GetWorld()->SpawnActor<AImperialPresentationEffects>();
    GetWorld()->SpawnActor<AImperialRoleRibbons>();
    if(FParse::Param(FCommandLine::Get(),TEXT("ImperialInputSmoke")))GetWorld()->SpawnActor<AImperialInputSafetyVerifier>();
    Super::StartPlay();
}
