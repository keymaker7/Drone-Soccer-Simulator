// Deterministic input-mapping fixtures. No device input or physical flight is simulated.
#include "ManualThrottleResponse.h"

#include <cmath>
#include <iostream>
#include <limits>

namespace
{
int Checks = 0;
int Failures = 0;
void Check(bool Value, const char* Name)
{
    ++Checks;
    if (!Value) { ++Failures; std::cerr << "FAIL: " << Name << '\n'; }
}
bool Near(double A, double B, double Tolerance = 1e-12) { return std::abs(A - B) <= Tolerance; }
}

int main()
{
    using ImperialInput::ManualThrottleResponse;
    constexpr double M = .55;
    constexpr double H = 1e-6;
    for (const bool Precision : {false, true})
    {
        Check(Near(ManualThrottleResponse(0, Precision), 0), "minimum is preserved");
        Check(Near(ManualThrottleResponse(1, Precision), 1), "maximum is preserved");
        Check(Near(ManualThrottleResponse(M, Precision), M), "55 percent reference is preserved");
        Check(Near(ManualThrottleResponse(-10, Precision), 0), "negative input clamps to minimum");
        Check(Near(ManualThrottleResponse(10, Precision), 1), "input above one clamps to maximum");
        Check(Near(ManualThrottleResponse(std::numeric_limits<double>::quiet_NaN(), Precision), 0), "NaN fails to zero");
        Check(Near(ManualThrottleResponse(std::numeric_limits<double>::infinity(), Precision), 0), "positive infinity fails to zero");
        Check(Near(ManualThrottleResponse(-std::numeric_limits<double>::infinity(), Precision), 0), "negative infinity fails to zero");
        bool Bounded = true, StrictlyIncreasing = true, DirectIdentity = true;
        double Previous = -1;
        for (int I = 0; I <= 100000; ++I)
        {
            const double U = I / 100000.0;
            const double V = ManualThrottleResponse(U, Precision);
            Bounded &= std::isfinite(V) && V >= 0 && V <= 1;
            StrictlyIncreasing &= V > Previous;
            DirectIdentity &= Precision || V == U;
            Previous = V;
        }
        Check(Bounded, "100001 input positions stay finite and bounded");
        Check(StrictlyIncreasing, "100001 input positions are strictly monotonic");
        Check(DirectIdentity, "direct mode is an identity map across full travel");
    }
    const double LeftSlope = (ManualThrottleResponse(M, true) - ManualThrottleResponse(M-H, true)) / H;
    const double RightSlope = (ManualThrottleResponse(M+H, true) - ManualThrottleResponse(M, true)) / H;
    Check(Near(LeftSlope, .65, 1e-8), "precision has 0.65 left slope at reference");
    Check(Near(RightSlope, .65, 1e-8), "precision has 0.65 right slope at reference");
    Check(Near(LeftSlope, RightSlope, 1e-8), "both halves meet without a slope discontinuity");
    Check(ManualThrottleResponse(.45, true) > .45 && ManualThrottleResponse(.45, true) < M,
          "input below reference moves toward reference without crossing it");
    Check(ManualThrottleResponse(.65, true) < .65 && ManualThrottleResponse(.65, true) > M,
          "input above reference moves toward reference without crossing it");
    const double DirectStep = ManualThrottleResponse(.56, false) - ManualThrottleResponse(.54, false);
    const double PrecisionStep = ManualThrottleResponse(.56, true) - ManualThrottleResponse(.54, true);
    Check(PrecisionStep > 0 && PrecisionStep < DirectStep * .66,
          "small stick motion around reference makes a smaller positive command change");
    Check(ManualThrottleResponse(.99, true) > .98 && Near(ManualThrottleResponse(1, true), 1),
          "precision retains full endpoint authority");
    std::cout << "Manual throttle fixtures: " << Checks << " checks, " << Failures
              << " failures. No hardware calibration or flight performed.\n";
    return Failures == 0 ? 0 : 1;
}
