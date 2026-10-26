#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "DroneSoccerRules.h"
#include "ImperialMatchManager.generated.h"
class ASoccerDronePawn;
enum class EImperialMode { Manual, Mission, Hold, Returning };
UCLASS()
class IMPERIALWINGSSIMULATOR_API AImperialMatchManager : public AActor
{
    GENERATED_BODY()
public:
    AImperialMatchManager();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    const TArray<TObjectPtr<ASoccerDronePawn>>& GetDrones() const { return Drones; }
    ASoccerDronePawn* GetDrone(int32 Id) const;
    ASoccerDronePawn* GetSelectedDrone() const;
    int32 GetSelected() const { return Selected; }
    void SelectDrone(int32 Id);
    void SelectNext();
    void LaunchSwarm();
    void HoldFleet();
    void ReturnFleet();
    void ManualSelected();
    void ResetMatch();
    void StartMatch();
    bool IsArmingAllowed() const { return Rules.IsArmingAllowed(); }
    FString GetMatchStatus() const;
    int32 GetSetWins(int32 Team) const { return Rules.SetWins(Team); }
    bool IsOffside(int32 Team) const { return Rules.IsOffside(Team); }
    void ToggleArmSelected();
    void ChangeView(int32 View);
    bool ExportTelemetry();
    bool IsConnected() const { return bConnected; }
    FString GetSourceCommit() const { return SourceCommit; }
    FString GetModeName() const;
    FString GetNotice() const { return Notice; }
    int32 GetScore(int32 Team) const { return Rules.Score(Team); }
    double GetElapsed() const { return Rules.GetPhase()==drone_soccer::Phase::Practice?Elapsed:Rules.Remaining(); }
    float GetFPS() const { return SmoothFPS; }
    float GetClosestSeparation() const { return ClosestSeparation; }
    int32 GetConflicts() const { return Conflicts; }
    int32 GetView() const { return CameraView; }
    FLinearColor GetTeamColor(int32 Id) const;
    const TArray<FString>& GetEvents() const { return Events; }
private:
    UPROPERTY() TArray<TObjectPtr<ASoccerDronePawn>> Drones;
    TArray<FVector> Homes;
    TArray<bool> GoalReady;
    TArray<FVector> PreviousPositions;
    TArray<FString> Events;
    drone_soccer::MatchRules Rules;
    EImperialMode Mode=EImperialMode::Manual;
    int32 Selected=0;
    int32 CameraView=1;
    int32 BlueScore=0,OrangeScore=0,Conflicts=0;
    double Elapsed=0,RequestClock=0,LastResponse=-100;
    float ClosestSeparation=0,SmoothFPS=60;
    bool bConnected=false,bRequestPending=false,bSmoke=false,bSmokeDone=false,bDemo=false,bDemoStarted=false;
    int32 SmokePhase=0;
    float SmokeMaxHeight=0,SmokeMinHeight=10000;
    float SmokeHoverError=0,SmokeNetPeak=0,SmokeNetVisualPeak=0,SmokeNetRebound=0,SmokeNetMinY=0;
    bool bSmokeFloorWall=false,bSmokeNet=false,bSmokeGoal=false;
    FString SourceCommit;
    FString CaptureFile;
    FString SmokeResultFile;
    double CaptureAt=10;
    bool bCaptureRequested=false;
    TArray<double> FrameTimes;
    bool bProfileWritten=false;
    FString Notice=TEXT("Keyboard ready. Select AUTO FLEET for SDACS autonomous guidance.");
    void RequestGuidance();
    void OnGuidance(FHttpRequestPtr Request,FHttpResponsePtr Response,bool bSuccess);
    void Log(const FString& Message);
    void UpdateScoring();
    void SmokeTick();
};
