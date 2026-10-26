#include "ImperialSuspendedGoal.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "PhysicsEngine/PhysicsConstraintComponent.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UObject/ConstructorHelpers.h"

AImperialSuspendedGoal::AImperialSuspendedGoal()
{
    PrimaryActorTick.bCanEverTick=true;PrimaryActorTick.TickGroup=TG_PostPhysics;
    PhysicsRoot=CreateDefaultSubobject<USphereComponent>(TEXT("SuspendedGoalBody"));RootComponent=PhysicsRoot;
    // The tiny root shape is INSIDE the upper rim, never inside the scoring hole.
    PhysicsRoot->InitSphereRadius(2);PhysicsRoot->SetCollisionProfileName(TEXT("PhysicsActor"));
    PhysicsRoot->SetNotifyRigidBodyCollision(true);PhysicsRoot->SetUseCCD(true);PhysicsRoot->SetAngularDamping(1.4f);PhysicsRoot->SetLinearDamping(.08f);
    PhysicsRoot->OnComponentHit.AddDynamic(this,&AImperialSuspendedGoal::OnGoalHit);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    for(int32 I=0;I<64;++I)
    {
        const double A=(I+.5)*2*PI/64;
        const FVector Radial(0,FMath::Sin(A),FMath::Cos(A)),Tangent(0,FMath::Cos(A),-FMath::Sin(A));
        auto* Segment=CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("PhysicalGoalRim%02d"),I));
        Segment->SetupAttachment(PhysicsRoot);Segment->SetStaticMesh(Cube.Object);
        Segment->SetRelativeLocation(FVector(0,0,-40)+Radial*40);
        Segment->SetRelativeRotation(FRotationMatrix::MakeFromXY(Tangent,Radial).Rotator());
        Segment->SetRelativeScale3D(FVector((2*50*FMath::Tan(PI/64)+.03)/100.,.20,.16));
        Segment->SetMobility(EComponentMobility::Movable);Segment->SetCollisionProfileName(TEXT("PhysicsActor"));
        Segment->SetNotifyRigidBodyCollision(true);Segment->BodyInstance.bAutoWeld=true;Segment->OnComponentHit.AddDynamic(this,&AImperialSuspendedGoal::OnGoalHit);Segments.Add(Segment);
    }
    ReadyLights=CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("MovingGoalSignal"));ReadyLights->SetupAttachment(PhysicsRoot);
    ReadyLights->SetStaticMesh(Cube.Object);ReadyLights->SetCollisionEnabled(ECollisionEnabled::NoCollision);ReadyLights->SetCastShadow(false);
    Cables=CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("SuspensionCables"));Cables->SetupAttachment(PhysicsRoot);
    Cables->SetStaticMesh(Cylinder.Object);Cables->SetCollisionEnabled(ECollisionEnabled::NoCollision);Cables->SetAbsolute(true,true,true);
    Suspension=CreateDefaultSubobject<UPhysicsConstraintComponent>(TEXT("PhysicalSuspension"));Suspension->SetupAttachment(PhysicsRoot);Suspension->SetAbsolute(true,true,true);
}
void AImperialSuspendedGoal::BeginPlay()
{
    Super::BeginPlay();RestTop=PhysicsRoot->GetComponentLocation();RestRotation=PhysicsRoot->GetComponentQuat();RestCentre=RestTop+RestRotation.RotateVector(FVector(0,0,-40));
    ContactMaterial=NewObject<UPhysicalMaterial>(this);ContactMaterial->Friction=.3f;ContactMaterial->Restitution=.2f;
    ContactMaterial->SleepLinearVelocityThreshold=.05f;ContactMaterial->SleepAngularVelocityThreshold=.05f;ContactMaterial->SleepCounterThreshold=20;
    double Mass=12.5,Damping=1.4;FString Text;TSharedPtr<FJsonObject> Settings;
    if(FFileHelper::LoadFileToString(Text,*FPaths::Combine(FPaths::ProjectContentDir(),TEXT("Data/physics-defaults.json")))&&FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Settings)&&Settings.IsValid())
    {double Value=0;if(Settings->TryGetNumberField(TEXT("goal_mass_kg"),Value)&&FMath::IsFinite(Value))Mass=FMath::Clamp(Value,10.,15.);if(Settings->TryGetNumberField(TEXT("goal_suspension_angular_damping"),Value)&&FMath::IsFinite(Value))Damping=FMath::Clamp(Value,.1,10.);}
    // Chaos sums the mass of EVERY welded body. Overriding the root alone
    // leaves the 64 cube bodies at their density-derived masses (~90kg).
    // Assign the ring's chosen total across those original bodies, then rebuild
    // the compound inertia. Never change the mass only for a verification run.
    PhysicsRoot->BodyInstance.SetMassOverride(.001f,true);
    PhysicsRoot->SetPhysMaterialOverride(ContactMaterial);PhysicsRoot->SetSimulatePhysics(true);
    for(auto& Segment:Segments)
    {
        Segment->BodyInstance.SetMassOverride((Mass-.001)/Segments.Num(),true);
        Segment->SetPhysMaterialOverride(ContactMaterial);Segment->WeldTo(PhysicsRoot,NAME_None,true);
    }
    PhysicsRoot->BodyInstance.UpdateMassProperties();
    PhysicsRoot->SetAngularDamping(Damping);PhysicsRoot->SetPhysicsMaxAngularVelocityInDegrees(180);
    Suspension->SetWorldLocation(RestCentre+FVector(0,0,150));Suspension->SetWorldRotation(FRotator::ZeroRotator);
    Suspension->SetLinearXLimit(ELinearConstraintMotion::LCM_Locked,0);Suspension->SetLinearYLimit(ELinearConstraintMotion::LCM_Locked,0);Suspension->SetLinearZLimit(ELinearConstraintMotion::LCM_Locked,0);
    SetCompetitionStable(false);
    Suspension->SetDisableCollision(true);Suspension->SetConstrainedComponents(nullptr,NAME_None,PhysicsRoot,NAME_None);
    Cables->SetWorldTransform(FTransform::Identity);Cables->AddInstance(FTransform::Identity);Cables->AddInstance(FTransform::Identity);
}
void AImperialSuspendedGoal::Configure(int32 Index,UMaterialInterface* Frame,UMaterialInterface* Signal)
{
    GoalIndex=Index;for(auto& Segment:Segments)Segment->SetMaterial(0,Frame);Cables->SetMaterial(0,Frame);ReadyLights->SetMaterial(0,Signal);
    ReadyLights->ClearInstances();TArray<FTransform> Lights;
    for(int32 I=0;I<64;++I)
    {const double A=(I+.5)*2*PI/64;const FVector R(0,FMath::Sin(A),FMath::Cos(A)),T(0,FMath::Cos(A),-FMath::Sin(A));Lights.Emplace(FRotationMatrix::MakeFromXY(T,R).ToQuat(),FVector(Index==0?8.1:-8.1,0,-40)+R*40,FVector((2*50*FMath::Tan(PI/64)+.03)/100.,.18,.006));}
    ReadyLights->AddInstances(Lights,false,false,false);
}
FVector AImperialSuspendedGoal::GetCentre() const{return PhysicsRoot->GetComponentLocation()+PhysicsRoot->GetComponentQuat().RotateVector(FVector(0,0,-40));}
FQuat AImperialSuspendedGoal::GetRingRotation() const{return PhysicsRoot->GetComponentQuat();}
float AImperialSuspendedGoal::GetSwayCm() const{return FVector::Dist2D(GetCentre(),RestCentre);}
void AImperialSuspendedGoal::Tick(float Dt)
{
    Super::Tick(Dt);PeakSway=FMath::Max(PeakSway,GetSwayCm());
    for(int32 I=0;I<2;++I)
    {
        const float Y=I==0?-35:35;
        const FVector A=RestCentre+FVector(0,Y,150),B=PhysicsRoot->GetComponentLocation()+PhysicsRoot->GetComponentQuat().RotateVector(FVector(0,Y,-5));
        const FVector D=B-A;Cables->UpdateInstanceTransform(I,FTransform(FRotationMatrix::MakeFromZ(D).ToQuat(),(A+B)*.5,FVector(.0028,.0028,D.Size()/100.)),true,I==1,true);
    }
}
void AImperialSuspendedGoal::OnGoalHit(UPrimitiveComponent*,AActor*,UPrimitiveComponent*,FVector Impulse,const FHitResult&)
{
    const double Now=GetWorld()->GetTimeSeconds();if(Impulse.Size()*.01<.2||Now-LastImpactTime<.05)return;
    LastImpactTime=Now;++ImpactCount;
}
void AImperialSuspendedGoal::ResetGoal()
{
    PhysicsRoot->SetPhysicsLinearVelocity(FVector::ZeroVector);PhysicsRoot->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector);
    PhysicsRoot->SetWorldLocationAndRotation(RestTop,RestRotation,false,nullptr,ETeleportType::TeleportPhysics);PhysicsRoot->WakeAllRigidBodies();
    PeakSway=0;ImpactCount=0;LastImpactTime=-100;
}
void AImperialSuspendedGoal::SetCompetitionStable(bool bStable)
{
    // FIDA requires no side-to-side sway. Practice permits only a small
    // fore/aft pitch response; official match mode locks the suspension.
    Suspension->SetAngularSwing1Limit(EAngularConstraintMotion::ACM_Locked,0);
    Suspension->SetAngularTwistLimit(EAngularConstraintMotion::ACM_Locked,0);
    Suspension->SetAngularSwing2Limit(bStable?EAngularConstraintMotion::ACM_Locked:EAngularConstraintMotion::ACM_Limited,bStable?0:2);
    if(PhysicsRoot->IsSimulatingPhysics())PhysicsRoot->WakeAllRigidBodies();
}
bool AImperialSuspendedGoal::HasOpenPhysicalHoop() const
{
    return Segments.Num()==64&&PhysicsRoot->IsSimulatingPhysics()&&GetCentre().Z>200&&PhysicsRoot->GetMass()>=10&&PhysicsRoot->GetMass()<=15;
}
