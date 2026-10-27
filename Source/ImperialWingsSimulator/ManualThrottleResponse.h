#pragma once

#include <algorithm>
#include <cmath>

namespace ImperialInput
{
// Pilot input shaping only: this is not the motor thrust curve or altitude hold.
// Preserve minimum, maximum and the chosen 55% input reference. The derivative
// is positive throughout [0,1] and is 0.65 at the reference in precision mode.
inline double ManualThrottleResponse(double Input, bool Precision)
{
    if (!std::isfinite(Input)) return 0.0;
    const double U = std::clamp(Input, 0.0, 1.0);
    if (!Precision) return U;
    constexpr double Midpoint = 0.55;
    constexpr double Expo = 0.35;
    const double Offset = U - Midpoint;
    const double Span = U < Midpoint ? Midpoint : 1.0 - Midpoint;
    return std::clamp(Midpoint + (1.0 - Expo) * Offset + Expo * Offset * Offset * Offset / (Span * Span), 0.0, 1.0);
}
}
