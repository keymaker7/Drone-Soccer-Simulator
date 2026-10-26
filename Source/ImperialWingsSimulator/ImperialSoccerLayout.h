#pragma once
#include "CoreMinimal.h"

// FIDA fixes the starting line, not the five roles' lateral order. This project's
// mirrored 1..5 order is a training layout viewed from each team's pilot area.
namespace ImperialSoccerLayout
{
inline const TCHAR* RoleName(int32 Role)
{
    static const TCHAR* Names[]={TEXT("STRIKER"),TEXT("GUIDE"),TEXT("LIBERO"),TEXT("SWEEPER"),TEXT("KEEPER")};
    return Names[FMath::Clamp(Role,0,4)];
}
inline FLinearColor RoleColor(int32 Role)
{
    static const FLinearColor Colors[]={FLinearColor(.015f,1,.035f),FLinearColor(1,.025f,.30f),FLinearColor(.20f,.65f,1),FLinearColor(1,.80f,.015f),FLinearColor::White};
    return Colors[FMath::Clamp(Role,0,4)];
}
inline FVector StartPosition(int32 Team,int32 Role)
{
    const float Y=-250.f+FMath::Clamp(Role,0,4)*125.f;
    return FVector(Team==0?-650.f:650.f,Team==0?Y:-Y,35.f);
}
inline FVector PilotPosition(int32 Team,int32 Role)
{
    return FVector(Team==0?-950.f:950.f,StartPosition(Team,Role).Y,170.f);
}
}
