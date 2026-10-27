#pragma once
#include "CoreMinimal.h"
class FJsonObject;
/** Synchronous scene budget ledger. Discarded time is never replayed later. */
struct FImperialSimulationTime
{
    double GameSeconds=0,PhysicsSeconds=0,DroppedSeconds=0;
    double LastGameSeconds=0,LastPhysicsSeconds=0,MaximumGameSeconds=0;
    uint64 Frames=0,LimitedFrames=0;
    void Record(double GameDt,double PhysicsDt);
    TSharedRef<FJsonObject> Json() const;
};
