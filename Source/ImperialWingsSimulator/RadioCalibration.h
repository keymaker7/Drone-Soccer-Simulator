#pragma once

// Portable, engine-independent calibration rules. No hardware input is synthesized here.
#include <array>
#include <cstddef>
#include <deque>

namespace ImperialRadio
{
constexpr std::size_t AxisCount = 6;
constexpr std::size_t ChannelCount = 4;
using Axes = std::array<double, AxisCount>;
using AxisMask = std::array<bool, AxisCount>;

// Physical hand placement is distinct from the driver channel order. Guided
// capture still measures each channel instead of assuming USB axis indices.
enum class StickMode { Mode1 = 1, Mode2 = 2 };
bool IsValidStickMode(StickMode Mode);
// Left = 0, right = 1; unsupported mode/channel = -1.
int StickForChannel(StickMode Mode, int Channel);

enum class Error
{
    None, Disconnected, InvalidAxis, DuplicateAxis, UnavailableAxis, InvalidDeadzone,
    InsufficientSamples, InsufficientRange, InvalidCenter, ThrottleNotLow,
    NotCapturing, MissingRange, MissingNeutral, NotConfirmed, InvalidSample,
    AmbiguousMovement, EndpointNotHeld, InputNotStable, IncompleteSweep, InvalidStickMode
};
const char* ErrorText(Error Value);

struct AxisRange
{
    double Minimum = 0.0;
    double Maximum = 1.0;
    double Center = 0.5;
};

struct Profile
{
    StickMode Mode = StickMode::Mode2;
    // Channel order is roll, pitch, yaw, throttle. Axis order is X, Y, Z, R, U, V.
    std::array<int, ChannelCount> Mapping{{0, 1, 2, 3}};
    std::array<bool, ChannelCount> Inverted{{false, false, false, false}};
    std::array<AxisRange, AxisCount> Ranges;
    double Deadzone = 0.04;
    bool RangeCaptured = false;
    bool NeutralCaptured = false;
    bool ThrottleMinimumConfirmed = false;
    bool Confirmed = false;
};

struct Capture
{
    Axes Minimum{};
    Axes Maximum{};
    std::size_t Samples = 0;
    bool Active = false;
    bool Interrupted = false;
    void Begin(Profile& Value);
    Error Observe(const Axes& Sample, const AxisMask& Available, bool Connected);
    Error FinishRange(Profile& Value, const AxisMask& Available, bool Connected);
};

struct Controls
{
    double Roll = 0.0;
    double Pitch = 0.0;
    double Yaw = 0.0;
    double Throttle = 0.0;
    bool Valid = false;
};

Error ValidateMapping(const Profile& Value, const AxisMask& Available);
Error ValidateProfile(const Profile& Value, const AxisMask& Available, bool RequireConfirmation = true);
Error CaptureNeutral(Profile& Value, const Axes& Sample, const AxisMask& Available, bool Connected);
Error Confirm(Profile& Value, const AxisMask& Available, bool Connected);
Controls MapControls(const Profile& Value, const Axes& Sample, const AxisMask& Available, bool Connected);
bool CanEnable(const Profile& Value, const Axes& Sample, const AxisMask& Available, bool Connected);
double NormalizeDriverAxis(double Value, double Minimum, double Maximum);

enum class GuidedPhase { Idle, Ready, Capturing, Neutral, Review, Complete, Interrupted };

/** One physical stick at a time. Only sustained real readings define ranges.
 *  Confirming a step records the held positive end (the low end for throttle).
 *  Ready separates steps so returning the previous stick does not contaminate
 *  the next measurement. This object never enables input or writes a profile.
 */
class GuidedCapture
{
public:
    Error Begin(const AxisMask& Available, bool Connected, double Deadzone = 0.04, StickMode Mode = StickMode::Mode2);
    Error StartStep(bool Connected);
    Error Observe(const Axes& Sample, const AxisMask& Available, bool Connected, double TimeSeconds);
    Error FinishStep(bool Connected);
    Error FinishNeutral(bool Connected);
    Error ConfirmedProfile(Profile& Output, bool Connected) const;
    void MarkSaved();
    void Cancel();
    GuidedPhase Phase() const { return CurrentPhase; }
    int Channel() const { return CurrentChannel; }
    std::size_t SampleCount() const { return TotalSamples; }
    bool Active() const;
    const Profile& Candidate() const { return Value; }
    double ObservedMinimum(int Axis) const;
    double ObservedMaximum(int Axis) const;
private:
    struct TimedAxes { double Time; Axes Value; };
    GuidedPhase CurrentPhase = GuidedPhase::Idle;
    int CurrentChannel = 0;
    Profile Value;
    AxisMask Mask{};
    Axes StableMinimum{}, StableMaximum{};
    Axes LatestSample{}, StartingCenter{};
    bool HaveLatestSample = false;
    std::deque<TimedAxes> Window;
    std::size_t TotalSamples = 0;
    double LastTime = -1;
    bool WindowStats(double Seconds, double MinimumDuration, Axes& Median, Axes& Spread) const;
    void ResetStepSamples();
    Error Interrupt(Error Reason);
};
}
