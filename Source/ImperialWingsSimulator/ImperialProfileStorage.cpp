#include "ImperialProfileStorage.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"

#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#endif

bool ImperialProfileStorage::Save(const FString& Path, const FString& Contents)
{
#if PLATFORM_WINDOWS
    if (Path.IsEmpty()) return false;
    const FString FullPath = FPaths::ConvertRelativePathToFull(Path);
    if (!IFileManager::Get().MakeDirectory(*FPaths::GetPath(FullPath), true)) return false;
    // A unique sibling stays on the same volume and cannot collide with another
    // save. Never use FileManager::Move here: it pre-deletes the old destination.
    const FString TempPath = FullPath + TEXT(".tmp-") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
    const bool Saved = FFileHelper::SaveStringToFile(Contents, *TempPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM) &&
        ::MoveFileExW(*TempPath, *FullPath, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
    if (!Saved) IFileManager::Get().Delete(*TempPath, false, true, true);
    return Saved;
#else
    // The controller backend is Windows-only. Do not use a destructive fallback.
    return false;
#endif
}
