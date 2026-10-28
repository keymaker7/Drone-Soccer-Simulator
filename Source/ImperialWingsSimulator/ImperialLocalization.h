#pragma once
#include "CoreMinimal.h"

/** Display-only text; language preference is separate from controller calibration. */
namespace ImperialLocalization
{
    IMPERIALWINGSSIMULATOR_API FString Translate(const FString& Value);
    IMPERIALWINGSSIMULATOR_API bool IsKorean();
    /** Persist an explicit choice; keep the previous language if writing fails. */
    IMPERIALWINGSSIMULATOR_API bool SetKorean(bool bKorean);
}
