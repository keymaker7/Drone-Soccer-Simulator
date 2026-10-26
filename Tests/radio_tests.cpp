// Internal math fixtures only. These tests neither open a joystick nor create a
// user calibration profile, and cannot establish that a physical Boxer was calibrated.
#include "RadioCalibration.h"

#include <cmath>
#include <iostream>
#include <limits>
#include <string>

using namespace ImperialRadio;

namespace
{
int Checks = 0;
int Failures = 0;
const AxisMask AllAxes{{true, true, true, true, true, true}};

void Check(bool Condition, const char* Name)
{
    ++Checks;
    if (!Condition) { ++Failures; std::cerr << "FAIL: " << Name << '\n'; }
}

bool Near(double A, double B) { return std::abs(A - B) < 1e-9; }

Profile SweptProfile()
{
    Profile Value;
    Capture Session;
    Session.Begin(Value);
    for (int Index = 0; Index < 32; ++Index)
    {
        const double Position = Index % 2 ? 0.9 : 0.1;
        Axes Sample{{Position, Position, Position, Position, 0.5, 0.5}};
        Check(Session.Observe(Sample, AllAxes, true) == Error::None, "range sample accepted");
    }
    Check(Session.FinishRange(Value, AllAxes, true) == Error::None, "four distinct measured ranges accepted");
    Check(!Value.Confirmed && !Value.NeutralCaptured, "range capture alone is not calibrated");
    return Value;
}

Profile ReadyProfile()
{
    Profile Value = SweptProfile();
    const Axes Neutral{{0.4, 0.55, 0.6, 0.1, 0.5, 0.5}};
    Check(CaptureNeutral(Value, Neutral, AllAxes, true) == Error::None, "asymmetric physical neutral accepted");
    Check(Confirm(Value, AllAxes, true) == Error::None, "complete calibration confirmed");
    return Value;
}
}

int main()
{
    Profile Empty;
    Axes Neutral{{0.5, 0.5, 0.5, 0.0, 0.5, 0.5}};
    Check(!CanEnable(Empty, Neutral, AllAxes, true), "factory defaults never enable radio");
    Check(!MapControls(Empty, Neutral, AllAxes, true).Valid, "uncalibrated inputs are invalid");
    Check(Confirm(Empty, AllAxes, true) == Error::MissingRange, "confirm requires captured range");
    Check(CaptureNeutral(Empty, Neutral, AllAxes, true) == Error::MissingRange, "neutral cannot bypass range capture");

    Profile Mapping;
    Mapping.Mapping[0] = -1;
    Check(ValidateMapping(Mapping, AllAxes) == Error::InvalidAxis, "negative axis rejected");
    Mapping.Mapping[0] = 6;
    Check(ValidateMapping(Mapping, AllAxes) == Error::InvalidAxis, "axis past six rejected");
    Mapping.Mapping[0] = 1;
    Check(ValidateMapping(Mapping, AllAxes) == Error::DuplicateAxis, "duplicate channels rejected");
    Mapping.Mapping[0] = 0;
    AxisMask Missing = AllAxes; Missing[3] = false;
    Check(ValidateMapping(Mapping, Missing) == Error::UnavailableAxis, "unreported driver axis rejected");

    Capture Still;
    Still.Begin(Empty);
    for (int Index = 0; Index < 20; ++Index) Still.Observe(Neutral, AllAxes, true);
    Check(Still.FinishRange(Empty, AllAxes, true) == Error::InsufficientRange, "stationary connected hardware cannot calibrate");
    Check(!Empty.RangeCaptured && !Empty.Confirmed, "failed range capture does not validate profile");
    Capture Short;
    Short.Begin(Empty);
    Axes Low{{0.1, 0.1, 0.1, 0.1, 0.5, 0.5}}, High{{0.9, 0.9, 0.9, 0.9, 0.5, 0.5}};
    Short.Observe(Low, AllAxes, true); Short.Observe(High, AllAxes, true);
    Check(Short.FinishRange(Empty, AllAxes, true) == Error::InsufficientSamples, "two samples do not establish calibration");
    Capture Narrow;
    Narrow.Begin(Empty);
    for (int Index = 0; Index < 32; ++Index)
    {
        Axes Sample = Index % 2 ? Low : High;
        Sample[2] = Index % 2 ? 0.4 : 0.6;
        Narrow.Observe(Sample, AllAxes, true);
    }
    Check(Narrow.FinishRange(Empty, AllAxes, true) == Error::InsufficientRange, "one axis below 25 percent travel blocks all channels");

    Profile InvalidNeutral = SweptProfile();
    Axes EdgeNeutral{{0.1, 0.5, 0.5, 0.1, 0.5, 0.5}};
    Check(CaptureNeutral(InvalidNeutral, EdgeNeutral, AllAxes, true) == Error::InvalidCenter, "endpoint cannot be recorded as centered roll");
    EdgeNeutral[0] = 0.5; EdgeNeutral[3] = 0.5;
    Check(CaptureNeutral(InvalidNeutral, EdgeNeutral, AllAxes, true) == Error::ThrottleNotLow, "mid throttle cannot be confirmed as minimum");
    Check(!InvalidNeutral.NeutralCaptured && !InvalidNeutral.ThrottleMinimumConfirmed, "failed neutral capture leaves both confirmations false");

    Profile Ready = ReadyProfile();
    Axes Centered{{0.4, 0.55, 0.6, 0.1, 0.5, 0.5}};
    const Controls CenterOutput = MapControls(Ready, Centered, AllAxes, true);
    Check(CenterOutput.Valid && Near(CenterOutput.Roll, 0) && Near(CenterOutput.Pitch, 0) && Near(CenterOutput.Yaw, 0), "asymmetric centers normalize to zero");
    Check(Near(CenterOutput.Throttle, 0) && CanEnable(Ready, Centered, AllAxes, true), "confirmed minimum throttle permits explicit enable");
    Axes Endpoints{{0.9, 0.1, 0.9, 0.9, 0.5, 0.5}};
    Controls End = MapControls(Ready, Endpoints, AllAxes, true);
    Check(Near(End.Roll, 1) && Near(End.Pitch, -1) && Near(End.Yaw, 1) && Near(End.Throttle, 1), "asymmetric calibrated endpoints reach full output");
    Check(!CanEnable(Ready, Endpoints, AllAxes, true), "high throttle blocks activation");
    Axes Tiny = Centered; Tiny[0] += 0.005;
    Check(Near(MapControls(Ready, Tiny, AllAxes, true).Roll, 0), "center noise within deadzone is removed");
    Ready.Inverted[0] = true;
    Check(Near(MapControls(Ready, Endpoints, AllAxes, true).Roll, -1), "roll inversion changes direction");
    Ready.Inverted[3] = true;
    Check(CaptureNeutral(Ready, Endpoints, AllAxes, true) == Error::InvalidCenter, "throttle inversion cannot bypass centered attitude axes");
    Axes InvertedMinimum = Centered; InvertedMinimum[3] = 0.9;
    Check(CaptureNeutral(Ready, InvertedMinimum, AllAxes, true) == Error::None, "inverted throttle confirms physical high endpoint as command minimum");
    Check(Confirm(Ready, AllAxes, true) == Error::None && CanEnable(Ready, InvertedMinimum, AllAxes, true), "inverted low throttle activates safely");

    const Controls Lost = MapControls(Ready, Endpoints, AllAxes, false);
    Check(!Lost.Valid && Near(Lost.Roll, 0) && Near(Lost.Pitch, 0) && Near(Lost.Yaw, 0) && Near(Lost.Throttle, 0), "disconnect returns no stale control values");
    Check(!CanEnable(Ready, InvertedMinimum, AllAxes, false), "disconnected device never enables");
    Profile Disconnected = Ready;
    Check(Confirm(Disconnected, AllAxes, false) == Error::Disconnected && !Disconnected.Confirmed, "disconnect prevents new confirmation");
    Capture Interrupted;
    Interrupted.Begin(Disconnected);
    Interrupted.Observe(Low, AllAxes, true);
    Check(Interrupted.Observe(High, AllAxes, false) == Error::Disconnected, "disconnect interrupts range capture");
    Check(Interrupted.Observe(High, AllAxes, true) == Error::Disconnected, "reconnect does not silently resume interrupted calibration");
    Check(Interrupted.FinishRange(Disconnected, AllAxes, true) == Error::Disconnected, "interrupted data cannot confirm after reconnect");

    Axes NonFinite = InvertedMinimum; NonFinite[0] = std::numeric_limits<double>::quiet_NaN();
    Check(!MapControls(Ready, NonFinite, AllAxes, true).Valid, "NaN control sample rejected");
    NonFinite[0] = std::numeric_limits<double>::infinity();
    Check(!MapControls(Ready, NonFinite, AllAxes, true).Valid, "infinite control sample rejected");
    NonFinite[0] = 1.1;
    Check(!MapControls(Ready, NonFinite, AllAxes, true).Valid, "out of driver range sample rejected");
    Profile Bad = Ready; Bad.Deadzone = 0.5;
    Check(ValidateProfile(Bad, AllAxes) == Error::InvalidDeadzone, "excessive deadzone rejected");
    Bad.Deadzone = -0.01;
    Check(ValidateProfile(Bad, AllAxes) == Error::InvalidDeadzone, "negative deadzone rejected");
    Bad.Deadzone = std::numeric_limits<double>::quiet_NaN();
    Check(ValidateProfile(Bad, AllAxes) == Error::InvalidDeadzone, "NaN deadzone rejected");
    Bad = Ready; Bad.Ranges[0].Maximum = Bad.Ranges[0].Minimum;
    Check(ValidateProfile(Bad, AllAxes) == Error::InsufficientRange, "zero-span loaded calibration rejected");
    Bad = Ready; Bad.Ranges[0].Center = 1.0;
    Check(ValidateProfile(Bad, AllAxes) == Error::InvalidCenter, "invalid loaded center rejected");
    Bad = Ready; Bad.Confirmed = false;
    Check(ValidateProfile(Bad, AllAxes) == Error::NotConfirmed, "stored ranges alone cannot imply confirmation");

    Check(Near(NormalizeDriverAxis(32767.5, 0, 65535), 0.5), "driver range normalizes midpoint");
    Check(Near(NormalizeDriverAxis(-10, 0, 65535), 0), "driver underflow clamps to zero");
    Check(Near(NormalizeDriverAxis(999999, 0, 65535), 1), "driver overflow clamps to one");
    Check(Near(NormalizeDriverAxis(10, 10, 10), 0), "zero driver span never divides by zero");

    Ready.Inverted[0] = false;
    double Previous = -1.0;
    bool BoundedMonotonic = true;
    for (int Index = 0; Index <= 1000; ++Index)
    {
        Axes Sample = InvertedMinimum; Sample[0] = Index / 1000.0;
        const Controls Output = MapControls(Ready, Sample, AllAxes, true);
        if (!Output.Valid || !std::isfinite(Output.Roll) || Output.Roll < -1 || Output.Roll > 1 || Output.Roll < Previous) BoundedMonotonic = false;
        Previous = Output.Roll;
    }
    Check(BoundedMonotonic, "all 1001 positions are finite, bounded and monotonic across asymmetric center/deadzone");
    bool SafeInvalidMappings = true;
    for (int Axis = -100; Axis <= 100; ++Axis)
    {
        if (Axis >= 0 && Axis < 6) continue;
        Bad = Ready; Bad.Mapping[0] = Axis;
        if (MapControls(Bad, Centered, AllAxes, true).Valid) SafeInvalidMappings = false;
    }
    Check(SafeInvalidMappings, "out of bounds mapping cannot index a control array");
    std::cout << "Radio calibration fixtures: " << Checks << " checks, " << Failures << " failures. No hardware calibration performed.\n";
    return Failures == 0 ? 0 : 1;
}
