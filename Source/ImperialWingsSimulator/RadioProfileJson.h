#pragma once

#include "CoreMinimal.h"
#include "RadioCalibration.h"

class FJsonObject;

namespace ImperialRadio
{
// Shared by native profile persistence and explicit JSON-only tests. These
// functions never poll devices, write files, or enable a controller.
void WriteProfileMode(const TSharedRef<FJsonObject>& Entry, const Profile& Value);
bool ReadProfileMode(const TSharedPtr<FJsonObject>& Entry, Profile& Value);
}
