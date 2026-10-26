#include "RadioCalibration.h"

#include <algorithm>
#include <cmath>
#include <limits>

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
}
