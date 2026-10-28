#pragma once

#include "CoreMinimal.h"

/** Saves controller configuration without deleting an existing file on failure. */
namespace ImperialProfileStorage
{
    bool Save(const FString& Path, const FString& Contents);
}
