#include "PrototypeArenaBuilder.h"
#include "ImperialElasticNet.h"
#include "ImperialSuspendedGoal.h"
#include "ImperialSoccerLayout.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/RectLightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "UObject/ConstructorHelpers.h"

APrototypeArenaBuilder::APrototypeArenaBuilder()
{
    PrimaryActorTick.bCanEverTick = false;
    ArenaRoot = CreateDefaultSubobject<USceneComponent>(TEXT("ArenaRoot"));
    ArenaRoot->SetMobility(EComponentMobility::Static); RootComponent = ArenaRoot;
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    CubeMesh = Cube.Object; CylinderMesh = Cylinder.Object; SphereMesh = Sphere.Object;
}

UMaterialInstanceDynamic* APrototypeArenaBuilder::Material(FLinearColor Color, float Glow, UMaterialInterface* BaseOverride)
{
    UMaterialInterface* Base = BaseOverride?BaseOverride:(Color.A < 0.99f && NetMaterial ? NetMaterial.Get() : SurfaceMaterial.Get());
    if (!Base) Base = UMaterial::GetDefaultMaterial(MD_Surface);
    auto* Result = UMaterialInstanceDynamic::Create(Base, this);
    Result->SetVectorParameterValue(TEXT("Color"), Color);
    Result->SetScalarParameterValue(TEXT("Glow"), Glow);
    Result->SetScalarParameterValue(TEXT("Opacity"), Color.A);
    Materials.Add(Result); return Result;
}

void APrototypeArenaBuilder::SteelBeam(const FString& Name, FVector Start, FVector End, float Depth, float Width)
{
    const FVector Direction=End-Start;
    const FRotator Rotation=FRotationMatrix::MakeFromX(Direction).Rotator();
    const FVector Center=(Start+End)*.5;
    for(float Sign:{-1.f,1.f})
        Piece(Name+TEXT("Flange"),CubeMesh,StructureMaterial,Center+Rotation.RotateVector(FVector(0,0,Sign*Depth*.5)),FVector(Direction.Size(),Width,2.5),Rotation);
    Piece(Name+TEXT("Web"),CubeMesh,StructureMaterial,Center,FVector(Direction.Size(),2.5,Depth),Rotation);
}

void APrototypeArenaBuilder::Piece(const FString& Name, UStaticMesh* Mesh, UMaterialInterface* Mat,
    FVector PositionCm, FVector SizeCm, FRotator Rotation, bool bCollision, bool bInvisible)
{
    // Flexible, segmented instances replace only the old straight net wires.
    if(Name.StartsWith(TEXT("SideNet"))||Name.StartsWith(TEXT("StaticSideNet"))||
       Name.StartsWith(TEXT("EndNet"))||Name.StartsWith(TEXT("CeilingWire")))return;
    if (!Mesh) return;
    if (bCollision || bInvisible)
    {
        auto* Part = NewObject<UStaticMeshComponent>(this, MakeUniqueObjectName(this, UStaticMeshComponent::StaticClass(), FName(*Name)));
        AddInstanceComponent(Part); Part->SetupAttachment(ArenaRoot);
        Part->SetStaticMesh(Mesh); Part->SetMaterial(0, Mat);
        Part->SetRelativeLocation(PositionCm); Part->SetRelativeRotation(Rotation); Part->SetRelativeScale3D(SizeCm / 100.0);
        Part->SetMobility(EComponentMobility::Static);
        Part->SetCollisionEnabled(bCollision ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
        if (bCollision) Part->SetCollisionResponseToAllChannels(ECR_Block);
        if(bCollision)
        {
            UPhysicalMaterial* Physical=FloorPhysicalMaterial;
            if(Name.StartsWith(TEXT("BlueGoal"))||Name.StartsWith(TEXT("RedGoal")))Physical=GoalPhysicalMaterial;
            else if(Name==TEXT("EndCollider")||Name==TEXT("SideCollider")||Name==TEXT("CeilingCollider"))Physical=BackupPhysicalMaterial;
            if(Physical)Part->SetPhysMaterialOverride(Physical);
        }
        if (bInvisible) { Part->SetVisibility(false); Part->SetHiddenInGame(true); }
        Part->RegisterComponent(); return;
    }
    const FString Key = Mesh->GetName() + TEXT("_") + (Mat ? Mat->GetName() : TEXT("Default"));
    auto* Batch = Batches.FindRef(Key);
    if (!Batch)
    {
        Batch = NewObject<UInstancedStaticMeshComponent>(this);
        AddInstanceComponent(Batch); Batch->SetupAttachment(ArenaRoot);
        Batch->SetStaticMesh(Mesh); Batch->SetMaterial(0, Mat);
        Batch->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Batch->SetMobility(EComponentMobility::Static); Batch->SetCastShadow(Mat&&Mat->GetBlendMode()==BLEND_Opaque);
        Batch->RegisterComponent(); Batches.Add(Key, Batch);
    }
    const int32 Instance=Batch->AddInstance(FTransform(Rotation, PositionCm, SizeCm / 100.0), false);
    if(Name==TEXT("RoleStartPad")){StartPadBatches.Add(Batch);StartPadIndices.Add(Instance);}
}
bool APrototypeArenaBuilder::GetStartPadPosition(int32 AircraftId,FVector& Position) const
{
    if(!StartPadBatches.IsValidIndex(AircraftId)||!StartPadBatches[AircraftId].IsValid())return false;
    FTransform Transform;
    if(!StartPadBatches[AircraftId]->GetInstanceTransform(StartPadIndices[AircraftId],Transform,true))return false;
    Position=Transform.GetLocation();return true;
}

void APrototypeArenaBuilder::AddBox(const FString& Name, const FVector& PositionCm, const FVector& SizeCm, bool bCollision)
{
    Piece(Name, CubeMesh, StructureMaterial, PositionCm, SizeCm, FRotator::ZeroRotator, bCollision);
}

void APrototypeArenaBuilder::Label(const FString& Name, const FString& Value, FVector PositionCm, FRotator Rotation, float Size, FColor Color)
{
    auto* Text = NewObject<UTextRenderComponent>(this, MakeUniqueObjectName(this, UTextRenderComponent::StaticClass(), FName(*Name)));
    AddInstanceComponent(Text); Text->SetupAttachment(ArenaRoot);
    Text->SetRelativeLocation(PositionCm); Text->SetRelativeRotation(Rotation);
    Text->SetText(FText::FromString(Value)); Text->SetWorldSize(Size);
    Text->SetHorizontalAlignment(EHTA_Center); Text->SetVerticalAlignment(EVRTA_TextCenter);
    Text->SetTextRenderColor(Color); Text->SetCastShadow(false); Text->RegisterComponent();
}

void APrototypeArenaBuilder::AddGoal(const FString& Prefix, float X)
{
    const int32 GoalIndex=X<0?0:1;
    auto* ReadyLight=Material(FLinearColor(.90f,.94f,1.f),1.1f);
    GoalLights[GoalIndex]=ReadyLight;
    if(SuspendedGoals.Num()!=2)SuspendedGoals.SetNum(2);
    auto* Goal=GetWorld()->SpawnActor<AImperialSuspendedGoal>(FVector(X,0,340),FRotator::ZeroRotator);
    if(Goal){Goal->Configure(GoalIndex,GoalFrameMaterial,ReadyLight);SuspendedGoals[GoalIndex]=Goal;}
    Piece(Prefix+TEXT("GoalRoofBracket"),CubeMesh,StructureMaterial,FVector(X,0,455),FVector(12,90,8));
}
AImperialSuspendedGoal* APrototypeArenaBuilder::GetGoal(int32 Index) const{return SuspendedGoals.IsValidIndex(Index)?SuspendedGoals[Index].Get():nullptr;}
void APrototypeArenaBuilder::ResetGoals(){for(auto& Goal:SuspendedGoals)if(Goal)Goal->ResetGoal();}

void APrototypeArenaBuilder::SetGoalSignal(int32 GoalIndex,bool bScored)
{
    if(!GoalLights.IsValidIndex(GoalIndex)||!GoalLights[GoalIndex]||GoalIsScored[GoalIndex]==bScored)return;
    GoalIsScored[GoalIndex]=bScored;
    GoalLights[GoalIndex]->SetVectorParameterValue(TEXT("Color"),bScored?FLinearColor(1.f,.025f,.015f):FLinearColor(.90f,.94f,1.f));
}

void APrototypeArenaBuilder::AddLight(const FString& Name, const FVector& PositionCm)
{
    auto* Light = NewObject<URectLightComponent>(this, FName(*Name));
    AddInstanceComponent(Light); Light->SetupAttachment(ArenaRoot);
    Light->SetRelativeLocation(PositionCm); Light->SetIntensityUnits(ELightUnits::Lumens);
    Light->SetRelativeRotation(FRotator(-90,0,0));
    Light->SetSourceWidth(120);Light->SetSourceHeight(45);
    Light->SetIntensity(22000); Light->SetAttenuationRadius(2500);
    Light->SetLightColor(FLinearColor(1.f, .94f, .86f)); Light->SetCastShadows(true); Light->RegisterComponent();
}

void APrototypeArenaBuilder::BeginPlay()
{
    Super::BeginPlay();
    SurfaceMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_Surface.M_Surface"));
    NetMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_Zone.M_Zone"));
    CourtMaterial=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Materials/M_Court.M_Court"));
    ConcreteMaterial=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Materials/M_Concrete.M_Concrete"));
    MetalMaterial=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Materials/M_Metal.M_Metal"));
    RubberMaterial=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Materials/M_Rubber.M_Rubber"));
    FloorPhysicalMaterial=NewObject<UPhysicalMaterial>(this,TEXT("CourtPhysicalMaterial"));
    FloorPhysicalMaterial->StaticFriction=.8f;FloorPhysicalMaterial->Friction=.65f;FloorPhysicalMaterial->Restitution=.1f;
    GoalPhysicalMaterial=NewObject<UPhysicalMaterial>(this,TEXT("GoalPhysicalMaterial"));
    GoalPhysicalMaterial->StaticFriction=.3f;GoalPhysicalMaterial->Friction=.3f;GoalPhysicalMaterial->Restitution=.2f;
    BackupPhysicalMaterial=NewObject<UPhysicalMaterial>(this,TEXT("BackupPhysicalMaterial"));
    BackupPhysicalMaterial->StaticFriction=.05f;BackupPhysicalMaterial->Friction=.05f;BackupPhysicalMaterial->Restitution=.05f;
    BackupPhysicalMaterial->bOverrideFrictionCombineMode=true;BackupPhysicalMaterial->FrictionCombineMode=EFrictionCombineMode::Min;
    BackupPhysicalMaterial->bOverrideRestitutionCombineMode=true;BackupPhysicalMaterial->RestitutionCombineMode=EFrictionCombineMode::Min;
    ElasticNet=GetWorld()->SpawnActorDeferred<AImperialElasticNet>(AImperialElasticNet::StaticClass(),GetActorTransform(),this,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
    if(ElasticNet){ElasticNet->Configure(NetMaterial,bCreateCeiling);ElasticNet->FinishSpawning(GetActorTransform());}
    StructureMaterial = Material(FLinearColor(.10f, .12f, .14f),0,MetalMaterial);
    GoalFrameMaterial=Material(FLinearColor(.55f,.60f,.63f),0,MetalMaterial);
    BlueMaterial = Material(FLinearColor(.025f, .25f, .80f),.7f);
    RedMaterial = Material(FLinearColor(.80f, .035f, .025f),.7f);
    auto* Floor = Material(FLinearColor(.065f, .09f, .11f),0,CourtMaterial);
    auto* Hall = Material(FLinearColor(.22f, .235f, .24f),0,ConcreteMaterial);
    auto* Wall = Material(FLinearColor(.42f, .435f, .44f),0,ConcreteMaterial);
    auto* BlueHalf = Material(FLinearColor(.065f, .11f, .19f),0,CourtMaterial);
    auto* RedHalf = Material(FLinearColor(.19f, .075f, .065f),0,CourtMaterial);
    auto* Marking = Material(FLinearColor(.65f, .68f, .66f),0,CourtMaterial);
    auto* Light = Material(FLinearColor(.95f, .90f, .80f),1.2f);
    auto* MeshNet = Material(FLinearColor(.055f, .065f, .07f, .58f));
    auto* NearNet = Material(FLinearColor(.055f, .065f, .07f, .32f));
    auto* SeatBlue=Material(FLinearColor(.025f,.08f,.21f),0,RubberMaterial);
    auto* SeatRed=Material(FLinearColor(.23f,.035f,.025f),0,RubberMaterial);
    auto* BlackRubber=Material(FLinearColor(.025f,.028f,.03f),0,RubberMaterial);
    Piece(TEXT("HangarGround"), CubeMesh, Hall, FVector(0, 0, -38), FVector(6000, 5000, 45));
    Piece(TEXT("CourtFloor"), CubeMesh, Floor, FVector(0, 0, -10), FVector(1600, 700, 20), FRotator::ZeroRotator, true);
    Piece(TEXT("BlueHalf"), CubeMesh, BlueHalf, FVector(-399, 0, .5), FVector(796, 694, 1));
    Piece(TEXT("RedHalf"), CubeMesh, RedHalf, FVector(399, 0, .5), FVector(796, 694, 1));
    Piece(TEXT("Midline"), CubeMesh, Marking, FVector(0, 0, 1.5), FVector(4, 688, 1));
    for (int32 I = 0; I < 64; ++I)
    {
        const float A = I * 2.f * PI / 64.f;
        Piece(TEXT("CentreCircle"), CubeMesh, Marking, FVector(FMath::Cos(A) * 145, FMath::Sin(A) * 145, 1.7), FVector(15, 3, 1), FRotator(0, FMath::RadiansToDegrees(A) + 90, 0));
    }
    for (float X : {-795.f, 795.f}) Piece(TEXT("EndLine"), CubeMesh, Marking, FVector(X, 0, 2), FVector(3, 694, 2));
    for (float Y : {-345.f, 345.f}) Piece(TEXT("TouchLine"), CubeMesh, Marking, FVector(0, Y, 2), FVector(1590, 3, 2));
    for(int32 Team=0;Team<2;++Team)for(int32 RoleIndex=0;RoleIndex<5;++RoleIndex)
    {
        const FVector Home=ImperialSoccerLayout::StartPosition(Team,RoleIndex);
        const FLinearColor Color=ImperialSoccerLayout::RoleColor(RoleIndex);
        auto* Pad=Material(Color*.24f,0,CourtMaterial);
        auto* Rim=Material(Color,.22f);
        Piece(TEXT("RoleStartPad"),CylinderMesh,Pad,FVector(Home.X,Home.Y,1.12),FVector(54,54,.16));
        for(int32 Segment=0;Segment<36;++Segment)
        {
            const double A=Segment*2*PI/36.;
            Piece(TEXT("RolePadRim"),CubeMesh,Rim,FVector(Home.X+25.5*FMath::Cos(A),Home.Y+25.5*FMath::Sin(A),1.3),FVector(4.5,1.1,.12),FRotator(0,FMath::RadiansToDegrees(A)+90,0));
        }
        const float Forward=Team==0?1.f:-1.f;
        // Flat floor chevron and role label; no extra physical obstacle at takeoff.
        for(float Side:{-1.f,1.f})Piece(TEXT("StartFacingArrow"),CubeMesh,Rim,FVector(Home.X+Forward*37,Home.Y+Side*3,1.32),FVector(11,1.4,.10),FRotator(0,(Team==0?0.f:180.f)-Side*35.f,0));
        Label(TEXT("RoleStartLabel"),FString::Printf(TEXT("%d %s"),RoleIndex+1,ImperialSoccerLayout::RoleName(RoleIndex)),FVector(Home.X-Forward*35,Home.Y,1.4),FRotator(90,Team==0?0.f:180.f,0),7.f,Color.ToFColor(true));
    }
    for(float X:{-900.f,900.f})
    {
        Piece(TEXT("PilotZone"),CubeMesh,X<0?BlueHalf:RedHalf,FVector(X,0,-14),FVector(200,650,2));
        Piece(TEXT("PilotRearBoundary"),CubeMesh,Marking,FVector(X<0?-1000:1000,0,-12),FVector(3,650,2));
        for(float Y:{-250.f,-125.f,0.f,125.f,250.f})Piece(TEXT("PilotStandingMark"),CubeMesh,Marking,FVector(X,Y,-12),FVector(45,45,1));
        Label(TEXT("PilotAreaCaption"),X<0?TEXT("BLUE PILOT AREA"):TEXT("RED PILOT AREA"),FVector(X,0,-10),FRotator(90,0,-90),30,FColor(160,170,175));
    }
    // Nominal net planes stay at +/-800, +/-350,450cm. Backup inside faces are
    // 20cm farther out, leaving room for force-driven net/cage penetration.
    for (float X : {-826.f, 826.f}) Piece(TEXT("EndCollider"), CubeMesh, Floor, FVector(X, 0, 235), FVector(12, 764, 470), FRotator::ZeroRotator, true, true);
    for (float Y : {-376.f, 376.f}) Piece(TEXT("SideCollider"), CubeMesh, Floor, FVector(0, Y, 235), FVector(1664, 12, 470), FRotator::ZeroRotator, true, true);
    if (bCreateCeiling) Piece(TEXT("CeilingCollider"), CubeMesh, Floor, FVector(0, 0, 476), FVector(1664, 764, 12), FRotator::ZeroRotator, true, true);
    for (float Y : {-350.f, 350.f})
    {
        UMaterialInterface* Net = Y < 0 ? NearNet : MeshNet;
        for (int32 X = -800; X <= 800; X += 10) Piece(TEXT("SideNetVertical"), CubeMesh, Net, FVector(X, Y, 225), FVector(.28, .28, 450));
        for (int32 Z = 10; Z <= 450; Z += 10) Piece(TEXT("SideNetHorizontal"), CubeMesh, Net, FVector(0, Y, Z), FVector(1600, .28, .28));
        for (int32 X = -800; X <= 800; X += 400)
        {
            Piece(TEXT("NetPost"), CubeMesh, StructureMaterial, FVector(X, Y, 225), FVector(7, 7, 450));
            Piece(TEXT("NetPostCap"), CubeMesh, X < 0 ? BlueMaterial.Get() : RedMaterial.Get(), FVector(X, Y, 439), FVector(8, 8, 16));
            Piece(TEXT("PostFoot"),CubeMesh,BlackRubber,FVector(X,Y,3),FVector(22,22,6));
        }
        Piece(TEXT("TopNetRail"), CubeMesh, StructureMaterial, FVector(0, Y, 455), FVector(1640, 9, 9));
        Piece(TEXT("CourtSkirt"), CubeMesh, StructureMaterial, FVector(0, Y, 18), FVector(1640, 12, 36));
    }
    for (float X : {-800.f, 800.f})
    {
        for (int32 Y = -350; Y <= 350; Y += 10) Piece(TEXT("EndNetVertical"), CubeMesh, MeshNet, FVector(X, Y, 225), FVector(.28, .28, 450));
        for (int32 Z = 10; Z <= 450; Z += 10) Piece(TEXT("EndNetHorizontal"), CubeMesh, MeshNet, FVector(X, 0, Z), FVector(.28, 700, .28));
        Piece(TEXT("EndTopRail"), CubeMesh, StructureMaterial, FVector(X, 0, 455), FVector(9, 720, 9));
    }
    if(bCreateCeiling)
    {
        for(int32 X=-800;X<=800;X+=10)Piece(TEXT("CeilingWireY"),CubeMesh,MeshNet,FVector(X,0,450),FVector(.28,700,.28));
        for(int32 Y=-350;Y<=350;Y+=10)Piece(TEXT("CeilingWireX"),CubeMesh,MeshNet,FVector(0,Y,450),FVector(1600,.28,.28));
    }
    GoalLights.SetNum(2);AddGoal(TEXT("BlueGoal"), -650); AddGoal(TEXT("RedGoal"), 650);
    // The opaque hall roof is above the overview camera, so it cannot cover the court.
    // The elastic ceiling starts at4.5m, with the rigid backup20cm farther out.
    Piece(TEXT("RearWall"),CubeMesh,Wall,FVector(0,1470,650),FVector(4400,35,1300));
    for(float X:{-2200.f,2200.f})Piece(TEXT("HallSideWall"),CubeMesh,Wall,FVector(X,-150,650),FVector(35,3270,1300));
    Piece(TEXT("HallRoof"),CubeMesh,Wall,FVector(0,-150,1390),FVector(4440,3300,25));
    auto* Acoustic=Material(FLinearColor(.09f,.11f,.12f),0,RubberMaterial);
    for(int32 X=-1800;X<=1800;X+=300)
    {
        Piece(TEXT("WallAcousticPanel"),CubeMesh,Acoustic,FVector(X,1446,660),FVector(220,14,310));
        SteelBeam(TEXT("Column"),FVector(X,1400,0),FVector(X,1400,1300),28,20);
    }
    for(int32 Y=-1400;Y<=1200;Y+=650)
    {
        for(float X:{-2100.f,2100.f})SteelBeam(TEXT("SideColumn"),FVector(X,Y,0),FVector(X,Y,1300),32,22);
        SteelBeam(TEXT("TrussBottom"),FVector(-2100,Y,1200),FVector(2100,Y,1200),26,18);
        SteelBeam(TEXT("TrussTop"),FVector(-2100,Y,1330),FVector(2100,Y,1330),26,18);
        for(int32 X=-2100;X<2100;X+=350)
            SteelBeam(TEXT("TrussDiagonal"),FVector(X,Y,1200),FVector(X+350,Y,1330),10,8);
    }
    for(float X:{-1850.f,1850.f})
    {
        Piece(TEXT("ServiceDoor"),CubeMesh,StructureMaterial,FVector(X,1444,140),FVector(180,16,280));
        Piece(TEXT("DoorFrame"),CubeMesh,GoalFrameMaterial,FVector(X-94,1433,145),FVector(8,12,290));
        Piece(TEXT("DoorFrame"),CubeMesh,GoalFrameMaterial,FVector(X+94,1433,145),FVector(8,12,290));
        Piece(TEXT("DoorFrame"),CubeMesh,GoalFrameMaterial,FVector(X,1433,288),FVector(196,12,8));
        Piece(TEXT("DoorHandle"),CylinderMesh,GoalFrameMaterial,FVector(X+55,1428,135),FVector(3,3,28));
        Label(TEXT("ExitSign"),TEXT("EXIT"),FVector(X,1425,322),FRotator(0,-90,0),25,FColor(110,180,125));
    }
    Piece(TEXT("RearBrandPanel"),CubeMesh,StructureMaterial,FVector(0,1432,520),FVector(1540,16,190));
    Label(TEXT("HangarBrand"),TEXT("IMPERIAL WINGS"),FVector(0,1419,540),FRotator(0,-90,0),88,FColor(210,215,210));
    Label(TEXT("HangarSubtitle"),TEXT("GWANGJU  /  DRONE SOCCER"),FVector(0,1418,465),FRotator(0,-90,0),26,FColor(105,170,175));
    for (float Y : {-740.f, 760.f}) for (int32 Row = 0; Row < 3; ++Row)
    {
        const float Side = Y < 0 ? -1.f : 1.f;
        const float SeatY = Y + Side * Row * 70;
        Piece(TEXT("BleacherStep"), CubeMesh, StructureMaterial, FVector(0, SeatY, 14 + Row * 25), FVector(1600, 74, 25 + Row * 25));
        for (int32 Seat = 0; Seat < 20; ++Seat)
        {
            const float SeatX = -720 + Seat * 76;
            UMaterialInterface* Team = Seat < 10 ? SeatBlue : SeatRed;
            Piece(TEXT("StadiumSeat"), CubeMesh, Team, FVector(SeatX, SeatY, 40 + Row * 38), FVector(50, 44, 8));
            Piece(TEXT("SeatBack"), CubeMesh, Team, FVector(SeatX, SeatY + Side * 20, 58 + Row * 38), FVector(50, 6, 38),FRotator(-Side*8,0,0));
            for(float Offset:{-17.f,17.f})Piece(TEXT("SeatLeg"),CubeMesh,StructureMaterial,FVector(SeatX+Offset,SeatY,23+Row*38),FVector(4,32,26));
        }
    }
    for (float X : {-1100.f, 1100.f})
    {
        Piece(TEXT("SidePillar"), CubeMesh, StructureMaterial, FVector(X, 1000, 400), FVector(50, 70, 800));
        Piece(TEXT("PillarTeamPlate"),CubeMesh,X<0?BlueHalf:RedHalf,FVector(X,960,260),FVector(36,4,160));
    }
    if (bCreateLights)
    {
        for(float X:{-560.f,560.f})for(float Y:{-320.f,320.f})
        {
            const FVector P(X,Y,870);
            Piece(TEXT("LuminaireHousing"),CubeMesh,StructureMaterial,P+FVector(0,0,8),FVector(145,58,15));
            Piece(TEXT("LuminaireDiffuser"),CubeMesh,Light,P,FVector(130,46,2));
            Piece(TEXT("LampSuspension"),CylinderMesh,StructureMaterial,P+FVector(0,0,160),FVector(1.5,1.5,300));
            AddLight(FString::Printf(TEXT("ArenaLamp_%d_%d"),static_cast<int32>(X),static_cast<int32>(Y)),P-FVector(0,0,4));
        }
        auto* Key = GetWorld()->SpawnActor<ADirectionalLight>();
        Key->GetLightComponent()->SetMobility(EComponentMobility::Movable);
        Key->SetActorRotation(FRotator(-65, -35, 0));
        Key->GetLightComponent()->SetIntensity(.15f); Key->GetLightComponent()->SetLightColor(FLinearColor(.82f,.88f,1.f));
        Key->GetLightComponent()->SetCastShadows(false);
    }
}
