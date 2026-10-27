#include "RadioProfileJson.h"
#include "Dom/JsonObject.h"

namespace ImperialRadio
{
void WriteProfileMode(const TSharedRef<FJsonObject>& Entry, const Profile& Value)
{
    Entry->SetNumberField(TEXT("stick_mode"), static_cast<int>(Value.Mode));
}

bool ReadProfileMode(const TSharedPtr<FJsonObject>& Entry, Profile& Value)
{
    if (!Entry.IsValid()) return false;
    // v1 profiles created before selectable layouts used Mode 2. Do not change
    // their actual channel assignments, ranges, or confirmation flags.
    StickMode Mode = StickMode::Mode2;
    if (Entry->HasField(TEXT("stick_mode")))
    {
        double Number = 0;
        const TSharedPtr<FJsonValue>* Field = Entry->Values.Find(TEXT("stick_mode"));
        if (!Field || !Field->IsValid() || (*Field)->Type != EJson::Number ||
            !(*Field)->TryGetNumber(Number) || !FMath::IsFinite(Number) ||
            (Number != 1.0 && Number != 2.0)) return false;
        Mode = Number == 1.0 ? StickMode::Mode1 : StickMode::Mode2;
    }
    Value.Mode = Mode;
    return true;
}
}
