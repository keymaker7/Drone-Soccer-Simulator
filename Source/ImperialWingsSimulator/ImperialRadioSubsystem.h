#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Tickable.h"
#include "RadioCalibration.h"
#include "ImperialRadioSubsystem.generated.h"

/** The WinMM-reported identity is retained verbatim. It does not certify a brand or USB serial. */
struct FImperialRadioDevice
{
    int32 Id = INDEX_NONE;
    FString Name;
    FString Identity;
    FString RegistryKey;
    FString OemName;
    uint16 ManufacturerId = 0;
    uint16 ProductId = 0;
    int32 AxisCount = 0;
    int32 ButtonCount = 0;
    TArray<bool> AxisAvailable;
    TArray<uint32> AxisMinimum;
    TArray<uint32> AxisMaximum;
};

/** Native, explicitly selected USB joystick input. Every launch starts in keyboard mode. */
UCLASS()
class IMPERIALWINGSSIMULATOR_API UImperialRadioSubsystem : public UGameInstanceSubsystem, public FTickableGameObject
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    virtual void Tick(float DeltaTime) override;
    virtual bool IsTickable() const override;
    virtual bool IsTickableWhenPaused() const override { return true; }
    virtual TStatId GetStatId() const override;
    virtual UWorld* GetTickableGameObjectWorld() const override { return GetWorld(); }

    const TArray<FImperialRadioDevice>& GetDevices() const { return Devices; }
    const FImperialRadioDevice* GetSelectedDevice() const;
    const TArray<float>& GetRawAxes() const;
    uint32 GetButtons() const;
    float GetNormalizedRoll() const;
    float GetNormalizedPitch() const;
    float GetNormalizedYaw() const;
    float GetThrottle() const;
    bool IsConnected() const;
    bool IsCalibrated() const;
    bool IsEnabled() const;
    bool IsCalibrating() const { return bCalibrationActive; }
    FString GetStatus() const;
    int32 GetMappedAxis(int32 Channel) const;
    bool IsInverted(int32 Channel) const;
    float GetDeadzone() const { return static_cast<float>(Calibration.Deadzone); }
    float GetCapturedMinimum(int32 Axis) const;
    float GetCapturedMaximum(int32 Axis) const;
    int32 GetCaptureSampleCount() const { return static_cast<int32>(RangeCapture.Samples); }

    void RefreshDevices();
    bool SelectDevice(int32 Id);
    void CycleDevice();
    bool SetChannelMapping(int32 Channel, int32 Axis, bool bInvert);
    void CycleAxis(int32 Channel);
    void ToggleInvert(int32 Channel);
    bool SetDeadzone(float Deadzone);
    bool BeginCalibration();
    bool BeginRangeCapture() { return BeginCalibration(); }
    bool FinishRangeCapture();
    bool CaptureNeutral();
    bool FinishCalibration();
    bool SaveProfile();
    bool LoadProfile();
    bool EnableRadio();
    void DisableRadio();

private:
    TArray<FImperialRadioDevice> Devices;
    int32 SelectedId = INDEX_NONE;
    FString SelectedIdentity;
    TArray<float> RawAxes;
    TArray<float> EmptyAxes;
    uint32 Buttons = 0;
    ImperialRadio::Profile Calibration;
    ImperialRadio::Capture RangeCapture;
    bool bConnected = false;
    bool bEnabled = false;
    bool bCalibrationActive = false;
    bool bInitialized = false;
    double LastSampleTime = 0.0;
    double LastPollTime = 0.0;
    double LastEnumerationTime = 0.0;
    FString Status = TEXT("Keyboard input. Select a connected joystick to configure USB.");

    void PollSelectedDevice();
    void ClearLiveInput(const FString& Reason);
    void InvalidateCalibration();
    bool SetCalibrationStatus(ImperialRadio::Error Result, const FString& Success);
    ImperialRadio::Axes CurrentSample() const;
    ImperialRadio::AxisMask AvailableAxes() const;
    ImperialRadio::Controls CurrentControls() const;
    FString ProfilePath() const;
};
