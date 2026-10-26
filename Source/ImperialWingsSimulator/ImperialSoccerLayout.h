#pragma once
#include "CoreMinimal.h"

// FIDA fixes the starting line, not the five roles' lateral order. This project's
// mirrored training layout keeps the Striker at the middle of each starting line.
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
    // Left to right from either pilot area: Libero, Guide, Striker, Sweeper, Keeper.
    static const float LateralCm[] = {0.f, -125.f, -250.f, 125.f, 250.f};
    const float Y=LateralCm[FMath::Clamp(Role,0,4)];
    // Five-centimetre clearance above the 20cm collision radius. The old 35cm
    // pose dropped every fresh aircraft hard enough to consume guard integrity.
    return FVector(Team==0?-650.f:650.f,Team==0?Y:-Y,25.f);
}
inline FVector PilotPosition(int32 Team,int32 Role)
{
    return FVector(Team==0?-950.f:950.f,StartPosition(Team,Role).Y,170.f);
}
}
