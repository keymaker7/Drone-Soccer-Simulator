#include "ImperialElasticNet.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
constexpr float GridCm=10.f, SegmentCm=50.f, WireCm=.28f;
constexpr float RadiusCm=90.f, ImpactLifetime=.25f, MaximumCm=20.f;
constexpr int32 MaximumImpacts=32;
}
AImperialElasticNet::AImperialElasticNet()
{
    PrimaryActorTick.bCanEverTick=true;
    PrimaryActorTick.bStartWithTickEnabled=false;
    PrimaryActorTick.TickGroup=TG_PostPhysics;
    NetRoot=CreateDefaultSubobject<USceneComponent>(TEXT("ElasticNetRoot"));
    RootComponent=NetRoot;
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
    WireMesh=Cube.Object;
}
void AImperialElasticNet::Configure(UMaterialInterface* Material,bool bIncludeCeiling)
{
    BaseMaterial=Material;bCreateCeiling=bIncludeCeiling;
}
void AImperialElasticNet::BeginPlay()
{
    Super::BeginPlay();
    if(!BaseMaterial)BaseMaterial=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Materials/M_Zone.M_Zone"));
    if(!BaseMaterial)BaseMaterial=UMaterial::GetDefaultMaterial(MD_Surface);
    BuildGrid();
    UE_LOG(LogTemp,Display,TEXT("IMPERIAL_ELASTIC_NET_READY segments=%d surfaces=%d spacing_cm=10 max_deflection_m=0.20"),Segments.Num(),WireComponents.Num());
}
FTransform AImperialElasticNet::WireTransform(FVector A,FVector B)
{
    const FVector Direction=B-A;
    return FTransform(FRotationMatrix::MakeFromX(Direction).ToQuat(),(A+B)*.5,
        FVector(Direction.Size()/100.0,WireCm/100.0,WireCm/100.0));
}
FVector2D AImperialElasticNet::SurfacePoint(FVector P,int32 Surface)
{
    if(Surface<2)return FVector2D(P.X,P.Z);
    if(Surface<4)return FVector2D(P.Y,P.Z);
    return FVector2D(P.X,P.Y);
}
FVector AImperialElasticNet::OutwardNormal(int32 Surface)
{
    switch(Surface)
    {
    case 0:return FVector(0,-1,0);
    case 1:return FVector(0,1,0);
    case 2:return FVector(-1,0,0);
    case 3:return FVector(1,0,0);
    default:return FVector(0,0,1);
    }
}
float AImperialElasticNet::PinnedEdgeFactor(FVector P,int32 Surface)
{
    const FVector2D UV=SurfacePoint(P,Surface);
    const double HalfU=Surface==2||Surface==3?350.0:800.0;
    const double MinV=Surface==4?-350.0:0.0;
    const double MaxV=Surface==4?350.0:450.0;
    const double Edge=FMath::Min(FMath::Min(UV.X+HalfU,HalfU-UV.X),FMath::Min(UV.Y-MinV,MaxV-UV.Y));
    return static_cast<float>(FMath::Clamp(Edge/25.0,0.0,1.0));
}
void AImperialElasticNet::AddSegment(FVector A,FVector B,int32 Surface)
{
    if(!WireComponents.IsValidIndex(Surface))return;
    FSegment Segment;Segment.A=A;Segment.B=B;Segment.Surface=Surface;
    Segment.RestTransform=WireTransform(A,B);
    Segment.Instance=WireComponents[Surface]->AddInstance(Segment.RestTransform,false);
    const int32 Index=Segments.Add(Segment);
    const FVector2D UV=SurfacePoint((A+B)*.5,Surface);
    const FIntVector Key(FMath::FloorToInt(UV.X/SegmentCm),FMath::FloorToInt(UV.Y/SegmentCm),Surface);
    Buckets.FindOrAdd(Key).Add(Index);
}
void AImperialElasticNet::AddWire(FVector A,FVector B,int32 Surface)
{
    const int32 Count=FMath::Max(1,FMath::CeilToInt((B-A).Size()/SegmentCm));
    for(int32 I=0;I<Count;++I)
        AddSegment(FMath::Lerp(A,B,static_cast<double>(I)/Count),FMath::Lerp(A,B,static_cast<double>(I+1)/Count),Surface);
}
void AImperialElasticNet::BuildGrid()
{
    if(!WireMesh)return;
    const int32 Count=bCreateCeiling?5:4;
    for(int32 Surface=0;Surface<Count;++Surface)
    {
        auto* Wires=NewObject<UInstancedStaticMeshComponent>(this);
        AddInstanceComponent(Wires);Wires->SetupAttachment(NetRoot);
        Wires->SetStaticMesh(WireMesh);Wires->SetMobility(EComponentMobility::Movable);
        Wires->SetCollisionEnabled(ECollisionEnabled::NoCollision);Wires->SetGenerateOverlapEvents(false);
        Wires->SetCastShadow(false);Wires->SetCanEverAffectNavigation(false);
        auto* Material=UMaterialInstanceDynamic::Create(BaseMaterial,this);
        Material->SetVectorParameterValue(TEXT("Color"),FLinearColor(.055f,.065f,.07f,Surface==0?.32f:.58f));
        Material->SetScalarParameterValue(TEXT("Opacity"),Surface==0?.32f:.58f);
        Material->SetScalarParameterValue(TEXT("Glow"),0.f);
        Wires->SetMaterial(0,Material);WireMaterials.Add(Material);WireComponents.Add(Wires);
    }
    for(int32 Surface=0;Surface<2;++Surface)
    {
        const double Y=Surface==0?-350.0:350.0;
        for(int32 X=-800;X<=800;X+=static_cast<int32>(GridCm))AddWire(FVector(X,Y,0),FVector(X,Y,450),Surface);
        for(int32 Z=0;Z<=450;Z+=static_cast<int32>(GridCm))AddWire(FVector(-800,Y,Z),FVector(800,Y,Z),Surface);
    }
    for(int32 Surface=2;Surface<4;++Surface)
    {
        const double X=Surface==2?-800.0:800.0;
        for(int32 Y=-350;Y<=350;Y+=static_cast<int32>(GridCm))AddWire(FVector(X,Y,0),FVector(X,Y,450),Surface);
        for(int32 Z=0;Z<=450;Z+=static_cast<int32>(GridCm))AddWire(FVector(X,-350,Z),FVector(X,350,Z),Surface);
    }
    if(bCreateCeiling)
    {
        for(int32 X=-800;X<=800;X+=static_cast<int32>(GridCm))AddWire(FVector(X,-350,450),FVector(X,350,450),4);
        for(int32 Y=-350;Y<=350;Y+=static_cast<int32>(GridCm))AddWire(FVector(-800,Y,450),FVector(800,Y,450),4);
    }
    for(auto& Wires:WireComponents)Wires->RegisterComponent();
}
void AImperialElasticNet::SubmitImpact(FVector PointCm,FVector InwardNormal,float DeflectionMeters)
{
    if(PointCm.ContainsNaN()||InwardNormal.ContainsNaN()||!FMath::IsFinite(DeflectionMeters)||DeflectionMeters<=0)return;
    const FVector Normal=GetActorTransform().InverseTransformVectorNoScale(InwardNormal).GetSafeNormal();
    int32 Surface=INDEX_NONE;
    if(FMath::Abs(Normal.X)>.9)Surface=Normal.X>0?2:3;
    else if(FMath::Abs(Normal.Y)>.9)Surface=Normal.Y>0?0:1;
    else if(Normal.Z<-.9&&bCreateCeiling)Surface=4;
    if(Surface==INDEX_NONE)return;
    FImpact Incoming;Incoming.Point=GetActorTransform().InverseTransformPosition(PointCm);Incoming.Surface=Surface;
    Incoming.StrengthCm=FMath::Clamp(DeflectionMeters*100.f,0.f,MaximumCm);Incoming.Remaining=ImpactLifetime;
    const FVector2D UV=SurfacePoint(Incoming.Point,Surface);
    for(FImpact& Existing:Impacts)
        if(Existing.Surface==Surface&&FVector2D::DistSquared(SurfacePoint(Existing.Point,Surface),UV)<30.f*30.f)
        {
            Existing=Incoming;SetActorTickEnabled(true);return;
        }
    if(Impacts.Num()>=MaximumImpacts)
    {
        int32 Oldest=0;for(int32 I=1;I<Impacts.Num();++I)if(Impacts[I].Remaining<Impacts[Oldest].Remaining)Oldest=I;
        Impacts[Oldest]=Incoming;
    }
    else Impacts.Add(Incoming);
    SetActorTickEnabled(true);
}
FVector AImperialElasticNet::Displacement(FVector Point,int32 Surface) const
{
    const FVector2D UV=SurfacePoint(Point,Surface);
    double DistanceCm=0;
    const double Cutoff=FMath::Exp(-2.0);
    for(const FImpact& Impact:Impacts)if(Impact.Surface==Surface)
    {
        const double Squared=FVector2D::DistSquared(UV,SurfacePoint(Impact.Point,Surface));
        if(Squared>=RadiusCm*RadiusCm)continue;
        const double Gaussian=(FMath::Exp(-Squared/(2.0*45.0*45.0))-Cutoff)/(1.0-Cutoff);
        const double T=FMath::Clamp(static_cast<double>(Impact.Remaining/ImpactLifetime),0.0,1.0);
        const double Envelope=T*T*(3.0-2.0*T);
        DistanceCm+=Impact.StrengthCm*Gaussian*Envelope;
    }
    DistanceCm=FMath::Min(DistanceCm,static_cast<double>(MaximumCm))*PinnedEdgeFactor(Point,Surface);
    return OutwardNormal(Surface)*DistanceCm;
}
void AImperialElasticNet::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    const float Dt=FMath::Clamp(DeltaSeconds,0.f,.25f);
    for(FImpact& Impact:Impacts)Impact.Remaining-=Dt;
    Impacts.RemoveAll([](const FImpact& Impact){return Impact.Remaining<=0;});
    TSet<int32> Candidates=PreviouslyDeformed;
    // Midpoints are bucketed in each surface's own 2D coordinates. Search only
    // the Gaussian neighbourhood plus one half-segment for endpoint reach.
    for(const FImpact& Impact:Impacts)
    {
        const FVector2D UV=SurfacePoint(Impact.Point,Impact.Surface);
        constexpr float SearchCm=RadiusCm+SegmentCm*.5f;
        const int32 MinU=FMath::FloorToInt((UV.X-SearchCm)/SegmentCm),MaxU=FMath::FloorToInt((UV.X+SearchCm)/SegmentCm);
        const int32 MinV=FMath::FloorToInt((UV.Y-SearchCm)/SegmentCm),MaxV=FMath::FloorToInt((UV.Y+SearchCm)/SegmentCm);
        for(int32 U=MinU;U<=MaxU;++U)for(int32 V=MinV;V<=MaxV;++V)
            if(const TArray<int32>* Found=Buckets.Find(FIntVector(U,V,Impact.Surface)))for(int32 Index:*Found)Candidates.Add(Index);
    }
    TSet<int32> StillDeformed;
    bool DirtySurfaces[5]={false,false,false,false,false};
    MaximumCurrentDeflectionM=0;
    for(int32 Index:Candidates)
    {
        const FSegment& Segment=Segments[Index];
        const FVector A=Displacement(Segment.A,Segment.Surface),B=Displacement(Segment.B,Segment.Surface);
        const bool bDeformed=!A.IsNearlyZero(.001)||!B.IsNearlyZero(.001);
        if(!bDeformed&&!PreviouslyDeformed.Contains(Index))continue;
        const FTransform Transform=bDeformed?WireTransform(Segment.A+A,Segment.B+B):Segment.RestTransform;
        WireComponents[Segment.Surface]->UpdateInstanceTransform(Segment.Instance,Transform,false,false,true);
        DirtySurfaces[Segment.Surface]=true;
        if(bDeformed)
        {
            StillDeformed.Add(Index);
            MaximumCurrentDeflectionM=FMath::Max(MaximumCurrentDeflectionM,static_cast<float>(FMath::Max(A.Size(),B.Size())*.01));
        }
    }
    PreviouslyDeformed=MoveTemp(StillDeformed);
    for(int32 I=0;I<WireComponents.Num();++I)if(DirtySurfaces[I])WireComponents[I]->MarkRenderStateDirty();
    if(Impacts.IsEmpty()&&PreviouslyDeformed.IsEmpty())SetActorTickEnabled(false);
}
