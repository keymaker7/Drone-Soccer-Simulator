#pragma once
#include "CoreMinimal.h"

/** Per-user writable data; package contents and another pilot's calibration stay separate. */
namespace ImperialUserData
{
    FString Directory();
    FString Path(const FString& Relative);
}
