#include "SimulationTime.h"
#include "Dom/JsonObject.h"
void FImperialSimulationTime::Record(double GameDt,double PhysicsDt)
{
    LastGameSeconds=FMath::Max(0.,GameDt);LastPhysicsSeconds=FMath::Max(0.,PhysicsDt);
    GameSeconds+=LastGameSeconds;PhysicsSeconds+=LastPhysicsSeconds;
    const double Dropped=FMath::Max(0.,LastGameSeconds-LastPhysicsSeconds);
    DroppedSeconds+=Dropped;MaximumGameSeconds=FMath::Max(MaximumGameSeconds,LastGameSeconds);
    ++Frames;if(Dropped>1e-6)++LimitedFrames;
}
TSharedRef<FJsonObject> FImperialSimulationTime::Json() const
{
    auto O=MakeShared<FJsonObject>();O->SetStringField(TEXT("policy"),TEXT("sync_chaos_budget_discard_excess_no_catchup"));
    O->SetStringField(TEXT("match_clock"),TEXT("game_time"));O->SetStringField(TEXT("model_clock"),TEXT("scene_pre_tick_budget"));
    O->SetNumberField(TEXT("game_s"),GameSeconds);O->SetNumberField(TEXT("physics_budget_s"),PhysicsSeconds);
    O->SetNumberField(TEXT("dropped_physics_s"),DroppedSeconds);O->SetNumberField(TEXT("deferred_physics_s"),0);
    O->SetNumberField(TEXT("last_game_dt_s"),LastGameSeconds);O->SetNumberField(TEXT("last_physics_dt_s"),LastPhysicsSeconds);
    O->SetNumberField(TEXT("max_game_dt_s"),MaximumGameSeconds);O->SetNumberField(TEXT("frames"),(double)Frames);O->SetNumberField(TEXT("limited_frames"),(double)LimitedFrames);
    return O;
}
