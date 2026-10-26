#include "ImperialRoleRibbons.h"
#include "ImperialUserData.h"
#include "SoccerDronePawn.h"
#include "ImperialSoccerLayout.h"
#include "ImperialMatchManager.h"
#include "ImperialPlayerController.h"
#include "PrototypeArenaBuilder.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/PlatformMisc.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UObject/ConstructorHelpers.h"

AImperialRoleRibbons::AImperialRoleRibbons()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickGroup = TG_PostPhysics;
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("RoleRibbonRoot")));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (Cube.Succeeded()) StripMesh = Cube.Object;
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Surface(TEXT("/Game/Materials/M_Surface.M_Surface"));
    if (Surface.Succeeded()) StripMaterial = Surface.Object;
}

void AImperialRoleRibbons::BeginPlay()
{
    Super::BeginPlay();
    Discover();
}

void AImperialRoleRibbons::Discover()
{
    for (TActorIterator<ASoccerDronePawn> It(GetWorld()); It; ++It)
    {
        if (Aircraft.ContainsByPredicate([&](const auto& Set) { return Set.Drone.Get() == *It; })) continue;
        FRibbonSet Set; Set.Drone = *It; Aircraft.Add(Set);
    }
    for (int32 I = Aircraft.Num()-1; I >= 0; --I)
        if (!Aircraft[I].Drone.IsValid())
        {
            if (auto* Mesh = Aircraft[I].Mesh.Get()) Mesh->DestroyComponent();
            Aircraft.RemoveAtSwap(I);
        }
}

void AImperialRoleRibbons::Build(FRibbonSet& Set)
{
    auto* Drone = Set.Drone.Get();
    if (!Drone) return;
    Set.Role = Drone->GetPlayerRole();
    if (Set.Role > 1)
    {
        if (auto* Mesh = Set.Mesh.Get()) Mesh->SetVisibility(false);
        return;
    }
    auto* Mesh = Set.Mesh.Get();
    if (!Mesh)
    {
        Mesh = NewObject<UInstancedStaticMeshComponent>(this);
        AddInstanceComponent(Mesh);
        Mesh->SetupAttachment(Drone->GetBodyMesh());
        // The drone's body root is scaled0.4; these instances are authored in cm.
        Mesh->SetAbsolute(false, false, true);
        Mesh->SetRelativeScale3D(FVector::OneVector);
        Mesh->SetStaticMesh(StripMesh);
        Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Mesh->SetGenerateOverlapEvents(false);
        Mesh->SetCanEverAffectNavigation(false);
        Mesh->SetCastShadow(true);
        Mesh->ComponentTags.Add(TEXT("ImperialLowerRoleRibbons"));
        Mesh->RegisterComponent();
        Set.Mesh = Mesh;
        auto* Material = UMaterialInstanceDynamic::Create(StripMaterial, this);
        Material->SetScalarParameterValue(TEXT("Glow"), .025f);
        Mesh->SetMaterial(0, Material);
        Set.Material = Material;
        // Four flat ribbons, each composed of three contiguous flexible sections.
        for (int32 I = 0; I < 12; ++I) Mesh->AddInstance(FTransform::Identity);
    }
    Mesh->SetVisibility(true);
    if (auto* Material = Set.Material.Get())
        Material->SetVectorParameterValue(TEXT("Color"), ImperialSoccerLayout::RoleColor(Set.Role));
}

void AImperialRoleRibbons::Tick(float Dt)
{
    Super::Tick(Dt);
    Elapsed += Dt; DiscoveryClock += Dt;
    if (DiscoveryClock >= 1) { DiscoveryClock = 0; Discover(); }
    for (auto& Set : Aircraft)
    {
        auto* Drone = Set.Drone.Get();
        if (!Drone) continue;
        if (Set.Role != Drone->GetPlayerRole()) Build(Set);
        auto* Mesh = Set.Mesh.Get();
        if (!Mesh || Set.Role > 1) continue;
        const FVector LocalVelocity = Drone->GetActorQuat().UnrotateVector(Drone->GetBodyMesh()->GetPhysicsLinearVelocity());
        const float Motion = FMath::Clamp(LocalVelocity.Size() / 400.f + (Drone->IsArmed() ? .15f : 0.f), 0.f, 1.f);
        int32 Instance = 0;
        for (int32 Side : {-1, 1}) for (int32 Lane = 0; Lane < 2; ++Lane)
        {
            const float Flex = FMath::Clamp(-LocalVelocity.X * .008f, -4.f, 4.f)
                + FMath::Sin(Elapsed * 8 + Side + Lane) * Motion * 2.f;
            const FQuat Rotation(FVector::YAxisVector, FMath::DegreesToRadians(Flex));
            const FVector Anchor(8.f, Side * (8.f + Lane * 4.f), -10.f);
            for (int32 Segment = 0; Segment < 3; ++Segment)
            {
                const FVector LocalCentre = Anchor + Rotation.RotateVector(FVector(0, 0, -1.3f - Segment * 2.6f));
                const FTransform Transform(Rotation, LocalCentre, FVector(.0015f, .024f, .026f));
                Mesh->UpdateInstanceTransform(Instance, Transform, false, Instance == 11, true);
                ++Instance;
            }
        }
    }
    if (!bVerified && Elapsed >= 2 && FParse::Param(FCommandLine::Get(), TEXT("ImperialLayoutSmoke")))
    {
        bVerified = true; VerifyLayout();
    }
}

void AImperialRoleRibbons::VerifyLayout()
{
    auto* Match = Cast<AImperialMatchManager>(UGameplayStatics::GetActorOfClass(this, AImperialMatchManager::StaticClass()));
    auto* Arena = Cast<APrototypeArenaBuilder>(UGameplayStatics::GetActorOfClass(this, APrototypeArenaBuilder::StaticClass()));
    auto* PC = Cast<AImperialPlayerController>(UGameplayStatics::GetPlayerController(this, 0));
    bool bOk = Match && Arena && PC && Match->GetDrones().Num() == 10;
    TArray<TSharedPtr<FJsonValue>> Rows;
    if (bOk) for (int32 I = 0; I < 10; ++I)
    {
        auto* Drone = Match->GetDrone(I);
        const int32 RoleIndex = I % 5;
        const FVector Expected = ImperialSoccerLayout::StartPosition(I / 5, RoleIndex);
        FVector Pad;
        const bool bPad = Arena->GetStartPadPosition(I, Pad);
        const auto* Set = Aircraft.FindByPredicate([&](const auto& Item) { return Item.Drone.Get() == Drone; });
        const auto* Mesh = Set ? Set->Mesh.Get() : nullptr;
        const int32 Ribbons = Mesh && Mesh->IsVisible() ? Mesh->GetInstanceCount() / 3 : 0;
        const bool bCentred = RoleIndex != 0 || (FMath::Abs(Drone->GetActorLocation().Y) < 1 && FMath::Abs(Expected.Y) < .01f);
        Match->SelectDrone(I); Match->ChangeView(1);
        const FVector Pilot = PC->GetPilotStationPosition();
        const bool bPilot = FVector::Dist(Pilot, ImperialSoccerLayout::PilotPosition(I / 5, RoleIndex)) < .1f;
        // Reset is an explicit fixture action; verify real pawn reset origin too.
        Drone->ResetDrone();
        const FVector Reset = Drone->GetActorLocation();
        const bool bReset = FVector::DistXY(Reset, Expected) < .1f && !Drone->IsArmed();
        const bool bRow = bCentred && bPad && FVector::DistXY(Pad, Expected) < .1f && bPilot && bReset && Ribbons == (RoleIndex < 2 ? 4 : 0);
        bOk &= bRow;
        auto Row = MakeShared<FJsonObject>();
        Row->SetNumberField(TEXT("aircraft"), I + 1);
        Row->SetStringField(TEXT("role"), ImperialSoccerLayout::RoleName(RoleIndex));
        Row->SetNumberField(TEXT("start_x_cm"), Reset.X); Row->SetNumberField(TEXT("start_y_cm"), Reset.Y);
        Row->SetNumberField(TEXT("ribbons"), Ribbons);
        Row->SetBoolField(TEXT("ok"), bRow);
        Rows.Add(MakeShared<FJsonValueObject>(Row));
    }
    if (Match) { Match->SelectDrone(0); Match->ExportTelemetry(); }
    auto Report = MakeShared<FJsonObject>();
    Report->SetBoolField(TEXT("ok"), bOk); Report->SetArrayField(TEXT("aircraft"), Rows);
    Report->SetStringField(TEXT("scope"), TEXT("actual pads, pawn reset, LOS stations and ribbon instances; not scoring/cloth/detachment"));
    FString Json; FJsonSerializer::Serialize(Report, TJsonWriterFactory<>::Create(&Json));
    FString Path = FPaths::Combine(ImperialUserData::Directory(), TEXT("role-layout-smoke.json"));
    FParse::Value(FCommandLine::Get(), TEXT("ImperialLayoutResult="), Path);
    FFileHelper::SaveStringToFile(Json, *Path);
    UE_LOG(LogTemp, Display, TEXT("IMPERIAL_ROLE_LAYOUT ok=%d aircraft=%d"), bOk, Rows.Num());
    FPlatformMisc::RequestExit(false);
}
