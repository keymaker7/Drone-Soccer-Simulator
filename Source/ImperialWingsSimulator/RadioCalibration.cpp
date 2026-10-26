#include "RadioCalibration.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace ImperialRadio
{
namespace
{
constexpr double MinimumTravel = 0.25;
constexpr std::size_t MinimumSamples = 16;

bool ValidSample(const Axes& Sample, const AxisMask& Available)
{
    for (std::size_t Axis = 0; Axis < AxisCount; ++Axis)
    {
        if (Available[Axis] && (!std::isfinite(Sample[Axis]) || Sample[Axis] < 0.0 || Sample[Axis] > 1.0))
        {
            return false;
        }
    }
    return true;
}

double Centered(double Sample, const AxisRange& Range, bool Inverted, double Deadzone)
{
    const double Offset = Sample - Range.Center;
    double Value = Offset / (Offset >= 0.0 ? Range.Maximum - Range.Center : Range.Center - Range.Minimum);
    Value = std::clamp(Inverted ? -Value : Value, -1.0, 1.0);
    const double Magnitude = std::abs(Value);
    if (Magnitude <= Deadzone)
    {
        return 0.0;
    }
    return std::copysign((Magnitude - Deadzone) / (1.0 - Deadzone), Value);
}

double ThrottleValue(const Profile& Value, const Axes& Sample)
{
    const int Axis = Value.Mapping[3];
    const AxisRange& Range = Value.Ranges[static_cast<std::size_t>(Axis)];
    const double Position = std::clamp((Sample[static_cast<std::size_t>(Axis)] - Range.Minimum) /
                                     (Range.Maximum - Range.Minimum), 0.0, 1.0);
    return Value.Inverted[3] ? 1.0 - Position : Position;
}
}

const char* ErrorText(Error Value)
{
    switch (Value)
    {
    case Error::None: return "OK";
    case Error::Disconnected: return "Joystick disconnected or input stale. Select/reconnect and start again.";
    case Error::InvalidAxis: return "Select an axis from X, Y, Z, R, U, V for each channel.";
    case Error::DuplicateAxis: return "Roll, pitch, yaw and throttle must use four different axes.";
    case Error::UnavailableAxis: return "A mapped axis is not reported by the selected joystick.";
    case Error::InvalidDeadzone: return "Deadzone must be finite and between 0.00 and 0.25.";
    case Error::InsufficientSamples: return "Move the sticks through their full travel for longer before capturing neutral.";
    case Error::InsufficientRange: return "Each mapped axis needs at least 25% of the driver range. Sweep every stick fully.";
    case Error::InvalidCenter: return "Center roll, pitch and yaw; neutral must be within the middle 60% of measured travel.";
    case Error::ThrottleNotLow: return "Move physical throttle to minimum. Check the throttle axis and inversion.";
    case Error::NotCapturing: return "Start range capture before collecting calibration.";
    case Error::MissingRange: return "Full stick travel has not been captured.";
    case Error::MissingNeutral: return "Capture centered roll/pitch/yaw with physical throttle at minimum.";
    case Error::NotConfirmed: return "Finish and save calibration before enabling USB input.";
    case Error::InvalidSample: return "Input contains an invalid value; calibration was rejected.";
    case Error::AmbiguousMovement: return "More than one axis moved. Retry with only the indicated stick direction.";
    case Error::EndpointNotHeld: return "Hold the indicated final end, then confirm this step.";
    case Error::InputNotStable: return "Keep the sticks still for a moment before confirming.";
    case Error::IncompleteSweep: return "Sweep BOTH sides of the starting center. Retry from a centered stick.";
    }
    return "Unknown calibration error.";
}

double NormalizeDriverAxis(double Value, double Minimum, double Maximum)
{
    if (!std::isfinite(Value) || !std::isfinite(Minimum) || !std::isfinite(Maximum) || Maximum <= Minimum)
    {
        return 0.0;
    }
    return std::clamp((Value - Minimum) / (Maximum - Minimum), 0.0, 1.0);
}

Error ValidateMapping(const Profile& Value, const AxisMask& Available)
{
    AxisMask Used{};
    for (int Axis : Value.Mapping)
    {
        if (Axis < 0 || Axis >= static_cast<int>(AxisCount))
        {
            return Error::InvalidAxis;
        }
        if (Used[static_cast<std::size_t>(Axis)])
        {
            return Error::DuplicateAxis;
        }
        Used[static_cast<std::size_t>(Axis)] = true;
        if (!Available[static_cast<std::size_t>(Axis)])
        {
            return Error::UnavailableAxis;
        }
    }
    return Error::None;
}

Error ValidateProfile(const Profile& Value, const AxisMask& Available, bool RequireConfirmation)
{
    const Error MappingError = ValidateMapping(Value, Available);
    if (MappingError != Error::None)
    {
        return MappingError;
    }
    if (!std::isfinite(Value.Deadzone) || Value.Deadzone < 0.0 || Value.Deadzone > 0.25)
    {
        return Error::InvalidDeadzone;
    }
    if (!Value.RangeCaptured)
    {
        return Error::MissingRange;
    }
    for (std::size_t Channel = 0; Channel < ChannelCount; ++Channel)
    {
        const AxisRange& Range = Value.Ranges[static_cast<std::size_t>(Value.Mapping[Channel])];
        if (!std::isfinite(Range.Minimum) || !std::isfinite(Range.Maximum) || Range.Minimum < 0.0 ||
            Range.Maximum > 1.0 || Range.Maximum - Range.Minimum < MinimumTravel)
        {
            return Error::InsufficientRange;
        }
        if (Channel < 3)
        {
            const double CenterPosition = (Range.Center - Range.Minimum) / (Range.Maximum - Range.Minimum);
            if (!std::isfinite(CenterPosition) || CenterPosition < 0.2 || CenterPosition > 0.8)
            {
                return Error::InvalidCenter;
            }
        }
    }
    if (!Value.NeutralCaptured || !Value.ThrottleMinimumConfirmed)
    {
        return Error::MissingNeutral;
    }
    if (RequireConfirmation && !Value.Confirmed)
    {
        return Error::NotConfirmed;
    }
    return Error::None;
}

void Capture::Begin(Profile& Value)
{
    Minimum.fill(std::numeric_limits<double>::infinity());
    Maximum.fill(-std::numeric_limits<double>::infinity());
    Samples = 0;
    Active = true;
    Interrupted = false;
    Value.RangeCaptured = false;
    Value.NeutralCaptured = false;
    Value.ThrottleMinimumConfirmed = false;
    Value.Confirmed = false;
}

Error Capture::Observe(const Axes& Sample, const AxisMask& Available, bool Connected)
{
    if (!Connected || Interrupted)
    {
        Interrupted = true;
        Active = false;
        return Error::Disconnected;
    }
    if (!Active)
    {
        return Error::NotCapturing;
    }
    if (!ValidSample(Sample, Available))
    {
        Interrupted = true;
        Active = false;
        return Error::InvalidSample;
    }
    for (std::size_t Axis = 0; Axis < AxisCount; ++Axis)
    {
        if (Available[Axis])
        {
            Minimum[Axis] = std::min(Minimum[Axis], Sample[Axis]);
            Maximum[Axis] = std::max(Maximum[Axis], Sample[Axis]);
        }
    }
    ++Samples;
    return Error::None;
}

Error Capture::FinishRange(Profile& Value, const AxisMask& Available, bool Connected)
{
    if (!Connected || Interrupted)
    {
        return Error::Disconnected;
    }
    if (!Active)
    {
        return Error::NotCapturing;
    }
    const Error MappingError = ValidateMapping(Value, Available);
    if (MappingError != Error::None)
    {
        return MappingError;
    }
    if (Samples < MinimumSamples)
    {
        return Error::InsufficientSamples;
    }
    for (int Axis : Value.Mapping)
    {
        const std::size_t Index = static_cast<std::size_t>(Axis);
        if (!std::isfinite(Minimum[Index]) || !std::isfinite(Maximum[Index]) ||
            Maximum[Index] - Minimum[Index] < MinimumTravel)
        {
            return Error::InsufficientRange;
        }
    }
    for (std::size_t Axis = 0; Axis < AxisCount; ++Axis)
    {
        if (Available[Axis])
        {
            Value.Ranges[Axis].Minimum = Minimum[Axis];
            Value.Ranges[Axis].Maximum = Maximum[Axis];
            Value.Ranges[Axis].Center = (Minimum[Axis] + Maximum[Axis]) * 0.5;
        }
    }
    Value.RangeCaptured = true;
    Value.NeutralCaptured = false;
    Value.ThrottleMinimumConfirmed = false;
    Value.Confirmed = false;
    Active = false;
    return Error::None;
}

Error CaptureNeutral(Profile& Value, const Axes& Sample, const AxisMask& Available, bool Connected)
{
    Value.Confirmed = false;
    Value.NeutralCaptured = false;
    Value.ThrottleMinimumConfirmed = false;
    if (!Connected)
    {
        return Error::Disconnected;
    }
    const Error MappingError = ValidateMapping(Value, Available);
    if (MappingError != Error::None)
    {
        return MappingError;
    }
    if (!Value.RangeCaptured)
    {
        return Error::MissingRange;
    }
    if (!ValidSample(Sample, Available))
    {
        return Error::InvalidSample;
    }
    for (std::size_t Channel = 0; Channel < ChannelCount; ++Channel)
    {
        const int Axis = Value.Mapping[Channel];
        const AxisRange& Range = Value.Ranges[static_cast<std::size_t>(Axis)];
        if (!std::isfinite(Range.Minimum) || !std::isfinite(Range.Maximum) ||
            Range.Maximum - Range.Minimum < MinimumTravel)
        {
            return Error::InsufficientRange;
        }
        if (Channel < 3)
        {
            const double Position = (Sample[static_cast<std::size_t>(Axis)] - Range.Minimum) / (Range.Maximum - Range.Minimum);
            if (Position < 0.2 || Position > 0.8)
            {
                return Error::InvalidCenter;
            }
        }
    }
    if (ThrottleValue(Value, Sample) > 0.05)
    {
        return Error::ThrottleNotLow;
    }
    for (std::size_t Channel = 0; Channel < 3; ++Channel)
    {
        const std::size_t Axis = static_cast<std::size_t>(Value.Mapping[Channel]);
        Value.Ranges[Axis].Center = Sample[Axis];
    }
    Value.NeutralCaptured = true;
    Value.ThrottleMinimumConfirmed = true;
    return Error::None;
}

Error Confirm(Profile& Value, const AxisMask& Available, bool Connected)
{
    Value.Confirmed = false;
    if (!Connected)
    {
        return Error::Disconnected;
    }
    const Error Result = ValidateProfile(Value, Available, false);
    Value.Confirmed = Result == Error::None;
    return Result;
}

Controls MapControls(const Profile& Value, const Axes& Sample, const AxisMask& Available, bool Connected)
{
    Controls Output;
    if (!Connected || ValidateProfile(Value, Available) != Error::None || !ValidSample(Sample, Available))
    {
        return Output;
    }
    double* Channels[3] = {&Output.Roll, &Output.Pitch, &Output.Yaw};
    for (std::size_t Channel = 0; Channel < 3; ++Channel)
    {
        const std::size_t Axis = static_cast<std::size_t>(Value.Mapping[Channel]);
        *Channels[Channel] = Centered(Sample[Axis], Value.Ranges[Axis], Value.Inverted[Channel], Value.Deadzone);
    }
    Output.Throttle = ThrottleValue(Value, Sample);
    Output.Valid = true;
    return Output;
}

bool CanEnable(const Profile& Value, const Axes& Sample, const AxisMask& Available, bool Connected)
{
    const Controls Mapped = MapControls(Value, Sample, Available, Connected);
    return Mapped.Valid && Mapped.Throttle <= 0.05;
}

bool GuidedCapture::Active() const
{
    return CurrentPhase == GuidedPhase::Ready || CurrentPhase == GuidedPhase::Capturing ||
           CurrentPhase == GuidedPhase::Neutral || CurrentPhase == GuidedPhase::Review;
}

void GuidedCapture::ResetStepSamples()
{
    StableMinimum.fill(std::numeric_limits<double>::infinity());
    StableMaximum.fill(-std::numeric_limits<double>::infinity());
    Window.clear();
    TotalSamples = 0;
    LastTime = -1;
}

Error GuidedCapture::Interrupt(Error Reason)
{
    CurrentPhase = GuidedPhase::Interrupted;
    Value.Confirmed = false;
    Window.clear();
    return Reason;
}

Error GuidedCapture::Begin(const AxisMask& Available, bool Connected, double Deadzone)
{
    Cancel();
    if (!Connected) return Error::Disconnected;
    if (std::count(Available.begin(), Available.end(), true) < 4) return Error::UnavailableAxis;
    if (!std::isfinite(Deadzone) || Deadzone < 0 || Deadzone > 0.25) return Error::InvalidDeadzone;
    Value = Profile{};
    Value.Mapping.fill(-1);
    Value.Deadzone = Deadzone;
    Mask = Available;
    CurrentChannel = 0;
    CurrentPhase = GuidedPhase::Ready;
    ResetStepSamples();
    return Error::None;
}

Error GuidedCapture::StartStep(bool Connected)
{
    if (!Connected) return Interrupt(Error::Disconnected);
    if (CurrentPhase != GuidedPhase::Ready && CurrentPhase != GuidedPhase::Capturing) return Error::NotCapturing;
    if (!HaveLatestSample) return Error::InsufficientSamples;
    StartingCenter = LatestSample;
    ResetStepSamples();
    CurrentPhase = GuidedPhase::Capturing;
    return Error::None;
}

bool GuidedCapture::WindowStats(double Seconds, double MinimumDuration, Axes& Median, Axes& Spread) const
{
    if (Window.empty()) return false;
    const double Cutoff = Window.back().Time - Seconds;
    auto First = Window.begin();
    while (First != Window.end() && First->Time < Cutoff) ++First;
    const auto Count = static_cast<std::size_t>(std::distance(First, Window.end()));
    if (Count < 6 || Window.back().Time - First->Time < MinimumDuration) return false;
    for (std::size_t Axis = 0; Axis < AxisCount; ++Axis)
    {
        std::vector<double> Sorted;
        Sorted.reserve(Count);
        for (auto Item = First; Item != Window.end(); ++Item) Sorted.push_back(Item->Value[Axis]);
        std::sort(Sorted.begin(), Sorted.end());
        Median[Axis] = (Sorted[(Count - 1) / 2] + Sorted[Count / 2]) * 0.5;
        // Isolated driver initialization/outlier packets must not become endpoints.
        Spread[Axis] = Sorted[(Count - 1) * 9 / 10] - Sorted[(Count - 1) / 10];
    }
    return true;
}

Error GuidedCapture::Observe(const Axes& Sample, const AxisMask& Available, bool Connected, double TimeSeconds)
{
    if (!Active()) return Error::NotCapturing;
    if (!Connected) return Interrupt(Error::Disconnected);
    if (Available != Mask) return Interrupt(Error::UnavailableAxis);
    if (!ValidSample(Sample, Available) || !std::isfinite(TimeSeconds) || TimeSeconds < 0 ||
        (LastTime >= 0 && TimeSeconds <= LastTime)) return Interrupt(Error::InvalidSample);
    LastTime = TimeSeconds;
    LatestSample = Sample;
    HaveLatestSample = true;
    if (CurrentPhase == GuidedPhase::Ready || CurrentPhase == GuidedPhase::Review) return Error::None;
    Window.push_back({TimeSeconds, Sample});
    while (!Window.empty() && (Window.front().Time < TimeSeconds - 1.0 || Window.size() > 256)) Window.pop_front();
    ++TotalSamples;
    Axes Median{}, Spread{};
    if (CurrentPhase == GuidedPhase::Capturing && WindowStats(0.24, 0.18, Median, Spread))
    {
        for (std::size_t Axis = 0; Axis < AxisCount; ++Axis)
        {
            if (Mask[Axis] && Spread[Axis] <= 0.02)
            {
                StableMinimum[Axis] = std::min(StableMinimum[Axis], Median[Axis]);
                StableMaximum[Axis] = std::max(StableMaximum[Axis], Median[Axis]);
            }
        }
    }
    return Error::None;
}

double GuidedCapture::ObservedMinimum(int Axis) const
{
    return Axis >= 0 && Axis < static_cast<int>(AxisCount) && std::isfinite(StableMinimum[Axis]) ? StableMinimum[Axis] : 0;
}

double GuidedCapture::ObservedMaximum(int Axis) const
{
    return Axis >= 0 && Axis < static_cast<int>(AxisCount) && std::isfinite(StableMaximum[Axis]) ? StableMaximum[Axis] : 0;
}

Error GuidedCapture::FinishStep(bool Connected)
{
    if (!Connected) return Interrupt(Error::Disconnected);
    if (CurrentPhase != GuidedPhase::Capturing) return Error::NotCapturing;
    if (TotalSamples < MinimumSamples) return Error::InsufficientSamples;
    int Best = -1;
    double Travel = 0;
    for (int Axis = 0; Axis < static_cast<int>(AxisCount); ++Axis)
    {
        const double Span = ObservedMaximum(Axis) - ObservedMinimum(Axis);
        if (Mask[Axis] && Span > Travel) { Best = Axis; Travel = Span; }
    }
    if (Best < 0 || Travel < MinimumTravel) return Error::InsufficientRange;
    const double InitialPosition = (StartingCenter[Best] - ObservedMinimum(Best)) / Travel;
    if (CurrentChannel < 3 && (InitialPosition < 0.2 || InitialPosition > 0.8)) return Error::IncompleteSweep;
    for (int Previous = 0; Previous < CurrentChannel; ++Previous)
        if (Value.Mapping[Previous] == Best) return Error::DuplicateAxis;
    for (int Axis = 0; Axis < static_cast<int>(AxisCount); ++Axis)
        if (Axis != Best && Mask[Axis] && ObservedMaximum(Axis) - ObservedMinimum(Axis) > std::max(0.10, Travel * 0.25))
            return Error::AmbiguousMovement;
    Axes Median{}, Spread{};
    if (!WindowStats(0.45, 0.35, Median, Spread)) return Error::InsufficientSamples;
    if (Spread[Best] > 0.02) return Error::InputNotStable;
    const double Position = (Median[Best] - ObservedMinimum(Best)) / Travel;
    if (Position > 0.10 && Position < 0.90) return Error::EndpointNotHeld;
    Value.Mapping[CurrentChannel] = Best;
    // Attitude steps end at the positive stick end; throttle ends at physical low.
    Value.Inverted[CurrentChannel] = CurrentChannel == 3 ? Position >= 0.90 : Position <= 0.10;
    Value.Ranges[Best] = {ObservedMinimum(Best), ObservedMaximum(Best), (ObservedMinimum(Best) + ObservedMaximum(Best)) * 0.5};
    ++CurrentChannel;
    ResetStepSamples();
    if (CurrentChannel == static_cast<int>(ChannelCount))
    {
        Value.RangeCaptured = true;
        CurrentPhase = GuidedPhase::Neutral;
    }
    else CurrentPhase = GuidedPhase::Ready;
    return Error::None;
}

Error GuidedCapture::FinishNeutral(bool Connected)
{
    if (!Connected) return Interrupt(Error::Disconnected);
    if (CurrentPhase != GuidedPhase::Neutral) return Error::MissingRange;
    Axes Median{}, Spread{};
    if (!WindowStats(0.45, 0.35, Median, Spread)) return Error::InsufficientSamples;
    for (int Axis : Value.Mapping) if (Spread[Axis] > 0.02) return Error::InputNotStable;
    const Error Result = CaptureNeutral(Value, Median, Mask, true);
    if (Result == Error::None) CurrentPhase = GuidedPhase::Review;
    return Result;
}

Error GuidedCapture::ConfirmedProfile(Profile& Output, bool Connected) const
{
    if (!Connected) return Error::Disconnected;
    if (CurrentPhase != GuidedPhase::Review) return Error::NotConfirmed;
    Profile Candidate = Value;
    const Error Result = Confirm(Candidate, Mask, true);
    if (Result == Error::None) Output = Candidate;
    return Result;
}

void GuidedCapture::MarkSaved()
{
    if (CurrentPhase == GuidedPhase::Review)
    {
        Value.Confirmed = true;
        CurrentPhase = GuidedPhase::Complete;
    }
}

void GuidedCapture::Cancel()
{
    CurrentPhase = GuidedPhase::Idle;
    Value = Profile{};
    HaveLatestSample = false;
    CurrentChannel = 0;
    ResetStepSamples();
}
}
