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

// Timestamped mathematical fixtures, never connected to the native input service.
struct GuidedFixture
{
    GuidedCapture Session;
    Axes Sample{{0.5, 0.5, 0.0, 0.0, 0.0, 0.5}};
    double Time = 0;
    explicit GuidedFixture(StickMode Mode = StickMode::Mode2)
    {
        // These virtual sticks deliberately expose nonconsecutive axes. Their
        // physical hands stay fixed; only pitch/throttle placement changes.
        Sample[1] = Mode == StickMode::Mode1 ? .9 : .5;
        Sample[2] = Mode == StickMode::Mode1 ? .5 : .9;
        Session.Begin(AllAxes, true, .04, Mode);
        Time+=.01; Session.Observe(Sample,AllAxes,true,Time);
    }
    void Hold(int Count = 70)
    {
        for (int I = 0; I < Count; ++I) { Time += .01; Session.Observe(Sample, AllAxes, true, Time); }
    }
    void Sweep(int Axis, bool EndHigh)
    {
        Time+=.01;Session.Observe(Sample,AllAxes,true,Time);
        Session.StartStep(true);
        Sample[Axis] = 0.1; Hold();
        Sample[Axis] = 0.9; Hold();
        Sample[Axis] = EndHigh ? 0.9 : 0.1; Hold();
    }
    void FourSteps()
    {
        const bool Mode1 = Session.Candidate().Mode == StickMode::Mode1;
        const int Mapping[] = {0, Mode1 ? 2 : 1, 5, Mode1 ? 1 : 2};
        const bool EndHigh[] = {true, false, true, true};
        for (int I = 0; I < 4; ++I)
        {
            Sweep(Mapping[I], EndHigh[I]);
            Check(Session.FinishStep(true) == Error::None, "guided step discovers a distinct axis and held direction");
            Sample[Mapping[I]] = I == 3 ? 0.9 : 0.5;
            Hold(); // Ready observations cannot contaminate the following step.
        }
    }
};

void StickModeTests()
{
    Check(Profile{}.Mode == StickMode::Mode2, "legacy/default profile starts as Mode 2");
    const StickMode Invalid = static_cast<StickMode>(3);
    Check(!IsValidStickMode(Invalid), "unsupported mode rejected");
    Check(StickForChannel(Invalid, 0) == -1 && StickForChannel(StickMode::Mode1, -1) == -1 &&
          StickForChannel(StickMode::Mode2, 4) == -1, "invalid hand requests never guess a stick");
    GuidedCapture InvalidSession;
    Check(InvalidSession.Begin(AllAxes, true, .04, Invalid) == Error::InvalidStickMode && !InvalidSession.Active(),
          "unsupported mode cannot begin guided capture");

    for (const StickMode Mode : {StickMode::Mode1, StickMode::Mode2})
    {
        const bool Mode1 = Mode == StickMode::Mode1;
        Check(StickForChannel(Mode, 0) == 1 && StickForChannel(Mode, 2) == 0,
              "both modes use right roll and left yaw");
        Check(StickForChannel(Mode, 1) == (Mode1 ? 0 : 1) && StickForChannel(Mode, 3) == (Mode1 ? 1 : 0),
              "pitch and throttle physical hands follow selected mode");
        GuidedFixture Fixture(Mode);
        Check(Fixture.Session.Candidate().Mode == Mode, "guided candidate preserves selected layout");
        Profile Original; Original.Mode = Mode1 ? StickMode::Mode2 : StickMode::Mode1;
        const auto OriginalMapping = Original.Mapping;
        Fixture.Sweep(0, true);
        Fixture.Session.Cancel();
        Check(Fixture.Session.ConfirmedProfile(Original, true) == Error::NotConfirmed &&
              Original.Mapping == OriginalMapping && Original.Mode != Mode,
              "cancelled opposite-mode guide does not overwrite caller profile");

        GuidedFixture Complete(Mode);
        Complete.FourSteps();
        Check(Complete.Session.FinishNeutral(true) == Error::None, "both layouts accept centered axes and low throttle");
        Profile Captured;
        Check(Complete.Session.ConfirmedProfile(Captured, true) == Error::None && Captured.Mode == Mode,
              "explicit review exports calibrated layout and channels together");
        Check(Captured.Mapping == std::array<int,4>{{0, Mode1 ? 2 : 1, 5, Mode1 ? 1 : 2}},
              "USB channels are measured for each physical layout");
        Check(CanEnable(Captured, Complete.Sample, AllAxes, true), "both layouts require measured physical minimum throttle");
        const Controls Center = MapControls(Captured, Complete.Sample, AllAxes, true);
        Check(Center.Valid && Near(Center.Roll, 0) && Near(Center.Pitch, 0) && Near(Center.Yaw, 0) && Near(Center.Throttle, 0),
              "both layouts normalize neutral without a nonzero flight command");
        Axes Pitch = Complete.Sample;
        Pitch[Captured.Mapping[1]] = .1;
        const Controls Pitched = MapControls(Captured, Pitch, AllAxes, true);
        Check(Near(Pitched.Pitch, 1) && Near(Pitched.Throttle, 0), "forward pitch does not change throttle in either layout");
        Pitch[Captured.Mapping[1]] = .9;
        Check(Near(MapControls(Captured, Pitch, AllAxes, true).Pitch, -1), "backward pitch reaches the opposite full command");
        Axes Throttle = Complete.Sample;
        Throttle[Captured.Mapping[3]] = .1;
        const Controls Raised = MapControls(Captured, Throttle, AllAxes, true);
        Check(Near(Raised.Throttle, 1) && Near(Raised.Pitch, 0), "raising throttle does not change pitch in either layout");
        Check(!CanEnable(Captured, Throttle, AllAxes, true), "high physical throttle blocks both layouts despite neutral pitch");
        Throttle[Captured.Mapping[1]] = .9;
        Check(!CanEnable(Captured, Throttle, AllAxes, true), "lowering the wrong stick cannot satisfy low-throttle enable");
        Profile InvalidProfile = Captured; InvalidProfile.Mode = Invalid;
        Check(ValidateProfile(InvalidProfile, AllAxes) == Error::InvalidStickMode &&
              !MapControls(InvalidProfile, Complete.Sample, AllAxes, true).Valid,
              "unknown loaded layout cannot produce controls");
        Complete.Session.MarkSaved();
        Check(Complete.Session.Phase() == GuidedPhase::Complete && Complete.Session.Candidate().Mode == Mode,
              "saved guided completion retains its layout");
    }
}

void GuidedTests()
{
    GuidedFixture Ready;
    Profile Output; Output.Deadzone = .17;
    Check(Ready.Session.Phase() == GuidedPhase::Ready && Ready.Session.Candidate().Mapping[0] == -1, "guide begins without guessed channels");
    Check(Ready.Session.ConfirmedProfile(Output, true) == Error::NotConfirmed && Near(Output.Deadzone, .17), "incomplete guide leaves caller profile unchanged");
    Ready.FourSteps();
    Check(Ready.Session.Phase() == GuidedPhase::Neutral && !Ready.Session.Candidate().Confirmed, "all endpoints still require neutral and explicit confirmation");
    Check(Ready.Session.Candidate().Mapping == std::array<int, 4>{{0, 1, 5, 2}}, "nonconsecutive driver axes are found without defaults");
    Check(Ready.Session.Candidate().Inverted[1] && Ready.Session.Candidate().Inverted[3], "negative driver pitch and physical low-at-high throttle are inferred");
    Check(Ready.Session.FinishNeutral(true) == Error::None, "stable centered sticks and inverted minimum throttle pass");
    Check(Ready.Session.Phase() == GuidedPhase::Review && !Ready.Session.Candidate().Confirmed, "review does not silently confirm");
    Check(Ready.Session.ConfirmedProfile(Output, true) == Error::None && Output.Confirmed, "only review produces a validated candidate");
    Check(CanEnable(Output, Ready.Sample, AllAxes, true), "guided candidate maps physical low throttle to zero");
    Axes Positive = Ready.Sample; Positive[1] = .1;
    Check(Near(MapControls(Output, Positive, AllAxes, true).Pitch, 1), "held forward pitch maps to positive control");
    Ready.Session.MarkSaved();
    Check(Ready.Session.Phase() == GuidedPhase::Complete && !Ready.Session.Active(), "successful save closes the guide");

    GuidedFixture Stationary;
    Stationary.Session.StartStep(true); Stationary.Hold();
    Check(Stationary.Session.FinishStep(true) == Error::InsufficientRange, "stationary live device cannot pass a guide step");
    GuidedFixture Short;
    Short.Session.StartStep(true); Short.Hold(2);
    Check(Short.Session.FinishStep(true) == Error::InsufficientSamples, "rapid confirmation without observations is rejected");
    GuidedFixture Narrow;
    Narrow.Session.StartStep(true); Narrow.Sample[0] = .4; Narrow.Hold(); Narrow.Sample[0] = .6; Narrow.Hold();
    Check(Narrow.Session.FinishStep(true) == Error::InsufficientRange, "narrow sweep is rejected");
    GuidedFixture OneSided;
    OneSided.Session.StartStep(true);OneSided.Hold();OneSided.Sample[0]=.9;OneSided.Hold();
    Check(OneSided.Session.FinishStep(true)==Error::IncompleteSweep, "one-sided travel is rejected before the final neutral stage");
    OneSided.Sample[0]=.1;OneSided.Hold();OneSided.Sample[0]=.9;OneSided.Hold();
    Check(OneSided.Session.FinishStep(true)==Error::None, "sweeping the missing side repairs the current step");

    GuidedFixture Outlier;
    Outlier.Session.StartStep(true);
    Outlier.Hold();
    Outlier.Sample[3] = .5; Outlier.Hold(1); Outlier.Sample[3] = 0;
    Outlier.Sample[0] = .1; Outlier.Hold(); Outlier.Sample[0] = .9; Outlier.Hold();
    Check(Outlier.Session.FinishStep(true) == Error::None && Outlier.Session.Candidate().Mapping[0] == 0, "isolated unrelated driver packet does not create another moving axis");
    Check(Near(Outlier.Session.Candidate().Ranges[0].Minimum, .1), "only sustained endpoint values establish range");

    GuidedFixture Diagonal;
    Diagonal.Session.StartStep(true);
    Diagonal.Sample[0] = .1; Diagonal.Sample[1] = .1; Diagonal.Hold();
    Diagonal.Sample[0] = .9; Diagonal.Sample[1] = .9; Diagonal.Hold();
    Check(Diagonal.Session.FinishStep(true) == Error::AmbiguousMovement, "diagonal stick movement requires retry instead of guessing");
    Check(Diagonal.Session.Candidate().Mapping[0] == -1, "ambiguous step does not mutate mapping");
    Diagonal.Sample[0] = .5; Diagonal.Sample[1] = .5; Diagonal.Sweep(0, true);
    Check(Diagonal.Session.FinishStep(true) == Error::None, "retry clears contaminated range");
    Diagonal.Sample[0] = .5; Diagonal.Sweep(0, true);
    Check(Diagonal.Session.FinishStep(true) == Error::DuplicateAxis, "same physical axis cannot become two channels");

    GuidedFixture Middle;
    Middle.Sweep(0, true); Middle.Sample[0] = .5; Middle.Hold();
    Check(Middle.Session.FinishStep(true) == Error::EndpointNotHeld, "centered final stick cannot establish its positive direction");
    Middle.Sample[0] = .9; Middle.Hold();
    Check(Middle.Session.FinishStep(true) == Error::None, "holding the endpoint allows correction without losing good range");
    GuidedFixture Moving;
    Moving.Sweep(0, true);
    for (int I = 0; I < 60; ++I) { Moving.Sample[0] = I % 2 ? .1 : .9; Moving.Hold(1); }
    Check(Moving.Session.FinishStep(true) == Error::InputNotStable, "moving final stick cannot establish inversion");

    GuidedFixture Neutral;
    Neutral.FourSteps(); Neutral.Sample[2] = .5; Neutral.Hold();
    Check(Neutral.Session.FinishNeutral(true) == Error::ThrottleNotLow, "high throttle blocks guided neutral");
    Neutral.Sample[2] = .9; Neutral.Sample[0] = .1; Neutral.Hold();
    Check(Neutral.Session.FinishNeutral(true) == Error::InvalidCenter, "off-center attitude stick blocks guided neutral");
    Neutral.Sample[0] = .5; Neutral.Hold();
    Check(Neutral.Session.FinishNeutral(true) == Error::None, "neutral can be corrected without repeating all endpoints");
    Check(Neutral.Session.ConfirmedProfile(Output, false) == Error::Disconnected, "disconnect at final confirmation blocks profile output");

    GuidedFixture Lost;
    Lost.Session.StartStep(true); Lost.Hold();
    Check(Lost.Session.Observe(Lost.Sample, AllAxes, false, Lost.Time + .01) == Error::Disconnected && Lost.Session.Phase() == GuidedPhase::Interrupted, "disconnect interrupts guide");
    Check(Lost.Session.StartStep(true) == Error::NotCapturing, "reconnect cannot resume an interrupted step");
    Lost.Session.Begin(AllAxes, true); Lost.Session.StartStep(true);
    AxisMask Changed = AllAxes; Changed[4] = false;
    Check(Lost.Session.Observe(Lost.Sample, Changed, true, 1) == Error::UnavailableAxis, "device capability change interrupts guide");
    Lost.Session.Begin(AllAxes, true); Lost.Session.StartStep(true);
    Lost.Session.Observe(Lost.Sample, AllAxes, true, 1);
    Check(Lost.Session.Observe(Lost.Sample, AllAxes, true, .9) == Error::InvalidSample, "backwards time cannot validate stable endpoints");
    Lost.Session.Begin(AllAxes, true); Lost.Session.StartStep(true);
    Lost.Sample[0] = std::numeric_limits<double>::quiet_NaN();
    Check(Lost.Session.Observe(Lost.Sample, AllAxes, true, 1) == Error::InvalidSample, "invalid hardware sample interrupts guide");
    Lost.Session.Cancel();
    Check(Lost.Session.Phase() == GuidedPhase::Idle && !Lost.Session.Active(), "cancel discards candidate");
    Check(Lost.Session.Begin(AllAxes, false) == Error::Disconnected, "guide requires a connected device");
    Check(Lost.Session.Begin(AllAxes, true, .3) == Error::InvalidDeadzone, "guide checks deadzone");
    Check(Lost.Session.Begin(AxisMask{{true, true, true, false, false, false}}, true) == Error::UnavailableAxis, "guide requires four available axes");
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
    GuidedTests();
    StickModeTests();
    std::cout << "Radio calibration fixtures: " << Checks << " checks, " << Failures << " failures. No hardware calibration performed.\n";
    return Failures == 0 ? 0 : 1;
}
