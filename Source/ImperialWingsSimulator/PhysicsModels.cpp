#include "PhysicsModels.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace ImperialPhysics
{
namespace
{
constexpr double Pi = 3.14159265358979323846;
double Positive(double Value, double Fallback) { return std::isfinite(Value) && Value > 0 ? Value : Fallback; }
double Nonnegative(double Value) { return std::isfinite(Value) && Value > 0 ? Value : 0; }
double Response(double Dt, double Seconds)
{
    return Dt > 0 && std::isfinite(Dt) ? -std::expm1(-Dt / Positive(Seconds, 0.001)) : 0;
}
double Proximity(double Distance, double Range)
{
    if (!std::isfinite(Distance)) return 0;
    const double Weight = std::clamp(1.0 - std::max(0.0, Distance) / Positive(Range, 0.01), 0.0, 1.0);
    return Weight * Weight;
}
}

double ClampFinite(double Value, double Minimum, double Maximum, double Fallback)
{
    if (!std::isfinite(Minimum) || !std::isfinite(Maximum) || Maximum < Minimum) return 0;
    return std::clamp(std::isfinite(Value) ? Value : (std::isfinite(Fallback) ? Fallback : Minimum), Minimum, Maximum);
}
Vec3 AngleDesiredUp(double Roll, double Pitch, double MaximumTiltDegrees)
{
    const Vec3 Stick=ClampMagnitude({ClampFinite(Pitch,-1,1),ClampFinite(Roll,-1,1),0},1);
    const double Magnitude=Stick.Length();
    const double Tilt=ClampFinite(MaximumTiltDegrees,5,60,35)*Pi/180*Magnitude;
    return Magnitude>1e-9 ? Stick*(std::sin(Tilt)/Magnitude)+Vec3{0,0,std::cos(Tilt)} : Vec3{0,0,1};
}
Vec3 AngleBodyRate(const Vec3& DesiredUpInBody, double GainPerSecond, double MaximumRateRadS)
{
    if(!DesiredUpInBody.Finite()||DesiredUpInBody.Length()<1e-9)return {};
    const Vec3 Up=DesiredUpInBody/DesiredUpInBody.Length();
    const Vec3 Axis{-Up.Y,Up.X,0};
    const double Horizontal=Axis.Length();
    const double Angle=std::atan2(Horizontal,ClampFinite(Up.Z,-1,1));
    const double Speed=std::min(Angle*ClampFinite(GainPerSecond,0,30,4),ClampFinite(MaximumRateRadS,0,20,Pi));
    if(Horizontal>1e-9)return Axis*(Speed/Horizontal);
    return Up.Z<0?Vec3{Speed,0,0}:Vec3{};
}
Vec3 Vec3::operator/(double S) const { return std::isfinite(S) && std::abs(S) > 1e-15 ? *this * (1.0 / S) : Vec3{}; }
double Vec3::LengthSquared() const { return X * X + Y * Y + Z * Z; }
double Vec3::Length() const { return std::hypot(std::hypot(X, Y), Z); }
bool Vec3::Finite() const { return std::isfinite(X) && std::isfinite(Y) && std::isfinite(Z); }
Vec3 operator*(double S, const Vec3& V) { return V * S; }
double Dot(const Vec3& A, const Vec3& B) { return A.X * B.X + A.Y * B.Y + A.Z * B.Z; }
Vec3 Cross(const Vec3& A, const Vec3& B) { return {A.Y * B.Z - A.Z * B.Y, A.Z * B.X - A.X * B.Z, A.X * B.Y - A.Y * B.X}; }
Vec3 ClampMagnitude(const Vec3& V, double Maximum)
{
    if (!V.Finite() || !std::isfinite(Maximum) || Maximum <= 0) return {};
    const double Length = V.Length();
    return Length > Maximum ? V * (Maximum / Length) : V;
}

double MotorStep(MotorState& State, double Pwm, double Dt, const MotorParams& Params, double VoltageFactor, bool Enabled)
{
    const double Maximum = std::min(Positive(Params.MaxThrustN, 8.0), 1000000.0);
    const double Idle = ClampFinite(Params.IdleThrustN, 0, Maximum);
    const double Input = ClampFinite(Pwm, 0, 1);
    const double Voltage = ClampFinite(VoltageFactor, 0, 1.5);
    const double Exponent = Enabled ? ClampFinite(Params.PwmExponent, 0.5, 4.0, 2.0) : 1.0;
    const double Demand = Input > 0 ? (Idle + (Maximum - Idle) * std::pow(Input, Exponent)) * Voltage : 0;
    State.ThrustN = ClampFinite(State.ThrustN, 0, Maximum * 1.5);
    State.ThrustN += (Demand - State.ThrustN) * Response(Dt, Params.ResponseSeconds);
    State.OmegaRadS = std::min(Positive(Params.MaximumOmegaRadS, 1800), 1000000.0) * std::sqrt(std::max(0.0, State.ThrustN / Maximum));
    return State.ThrustN;
}

double PwmForThrust(double DesiredThrustN, const MotorParams& Params, double VoltageFactor, bool Nonlinear)
{
    const double Factor = ClampFinite(VoltageFactor, 0, 1.5);
    if (!std::isfinite(DesiredThrustN) || DesiredThrustN <= 0 || Factor <= 1e-9) return 0;
    const double Maximum = std::min(Positive(Params.MaxThrustN, 8.0), 1000000.0);
    const double Idle = ClampFinite(Params.IdleThrustN, 0, Maximum - 1e-9);
    const double Fraction = std::clamp((DesiredThrustN / Factor - Idle) / (Maximum - Idle), 0.0, 1.0);
    const double Exponent = Nonlinear ? ClampFinite(Params.PwmExponent, 0.5, 4.0, 2.0) : 1.0;
    // Distinguish armed idle from the PWM=0 stop command.
    return std::max(std::numeric_limits<double>::epsilon(), std::pow(Fraction, 1.0 / Exponent));
}

double EstimateRotorPowerW(double ThrustN, const MotorParams& Params, double DensityKgM3)
{
    const double Thrust = ClampFinite(ThrustN, 0, 1000);
    const double Radius = ClampFinite(Params.RotorRadiusM, 0.001, 1000.0, 0.055);
    const double Area = Pi * Radius * Radius;
    const double Efficiency = ClampFinite(Params.ElectricalEfficiency, 0.05, 1.0, 0.32);
    return std::pow(Thrust, 1.5) / (std::sqrt(2.0 * ClampFinite(DensityKgM3, 0.001, 1000000.0, 1.225) * Area) * Efficiency);
}

void BatteryStep(BatteryState& State, double ElectricalPowerW, double Dt, const BatteryParams& Params, bool Enabled)
{
    const double Nominal = Positive(Params.NominalVoltageV, 14.8);
    if (!Enabled)
    {
        State.VoltageV = Nominal; State.CurrentA = 0; State.DeliveredPowerW = 0; State.PowerLimited = false;
        return;
    }
    State.Soc = ClampFinite(State.Soc, 0, 1, 1);
    State.ConsumedAh = Nonnegative(State.ConsumedAh);
    const double Empty = Positive(Params.EmptyVoltageV, 13.2);
    const double Full = std::max(Empty, Positive(Params.FullVoltageV, 16.8));
    const double OpenCircuit = Empty + (Full - Empty) * State.Soc;
    if (State.Soc <= 0)
    {
        State.VoltageV = Empty; State.CurrentA = 0; State.DeliveredPowerW = 0; State.PowerLimited = ElectricalPowerW > 0;
        return;
    }
    const double Power = Nonnegative(ElectricalPowerW) + Nonnegative(Params.AvionicsCurrentA) * OpenCircuit;
    const double Resistance = Nonnegative(Params.InternalResistanceOhm);
    double Current;
    if (Resistance > 1e-12)
    {
        const double Discriminant = OpenCircuit * OpenCircuit - 4.0 * Resistance * Power;
        Current = Discriminant >= 0 ? 2.0 * Power / (OpenCircuit + std::sqrt(Discriminant)) : OpenCircuit / (2.0 * Resistance);
    }
    else Current = Power / OpenCircuit;
    Current = std::clamp(Current, 0.0, Positive(Params.MaximumCurrentA, 80));
    const double StepSeconds = Nonnegative(Dt);
    const double Capacity = Positive(Params.CapacityAh, 1.5);
    if (StepSeconds > 0) Current = std::min(Current, State.Soc * Capacity * 3600.0 / StepSeconds);
    State.VoltageV = std::max(0.0, OpenCircuit - Current * Resistance);
    State.CurrentA = Current;
    State.DeliveredPowerW = State.VoltageV * Current;
    State.PowerLimited = State.DeliveredPowerW + 1e-6 < Power;
    const double UsedAh = Current * StepSeconds / 3600.0;
    State.ConsumedAh += UsedAh;
    State.Soc = std::clamp(State.Soc - UsedAh / Capacity, 0.0, 1.0);
}

double BatteryThrustFactor(const BatteryState& State, const BatteryParams& Params, bool Enabled)
{
    if (!Enabled) return 1.0;
    if (!std::isfinite(State.Soc) || State.Soc <= 0) return 0;
    const double Ratio = Nonnegative(State.VoltageV) / Positive(Params.NominalVoltageV, 14.8);
    return ClampFinite(Ratio * Ratio, 0, 1.5);
}

Vec3 DragForce(const Vec3& RelativeAirVelocityMS, const AeroParams& Params, bool Enabled)
{
    if (!Enabled || !RelativeAirVelocityMS.Finite()) return {};
    const Vec3 Velocity = ClampMagnitude(RelativeAirVelocityMS, 1000);
    return Velocity * (-0.5 * ClampFinite(Params.DensityKgM3, 0, 1000000.0, 1.225) *
        ClampFinite(Params.EffectiveCdAreaM2, 0, 1000000.0) * Velocity.Length());
}

double SurfaceLiftFactor(double GroundDistanceM, double CeilingDistanceM, double WallDistanceM, const SurfaceParams& Params, bool Enabled)
{
    if (!Enabled) return 1.0;
    const double Range = Positive(Params.RotorRadiusM, 0.055) * Positive(Params.InfluenceRotorRadii, 4);
    const double Ground = Proximity(GroundDistanceM, Range) * ClampFinite(Params.GroundGain, 0, 1);
    const double Ceiling = Proximity(CeilingDistanceM, Range) * ClampFinite(Params.CeilingGain, 0, 1);
    const double Wall = Proximity(WallDistanceM, Range) * ClampFinite(Params.WallLoss, 0, 1);
    const double Minimum = ClampFinite(Params.MinimumFactor, 0.1, 1.0, 0.85);
    const double Maximum = ClampFinite(Params.MaximumFactor, 1.0, 1.5, 1.12);
    return std::clamp(1.0 + Ground + Ceiling - Wall, Minimum, Maximum);
}

Vec3 DownwashVelocity(const Vec3& UpperPositionM, const Vec3& SamplePositionM, double UpperTotalThrustN, const DownwashParams& Params, bool Enabled)
{
    if (!Enabled || !UpperPositionM.Finite() || !SamplePositionM.Finite() || !std::isfinite(UpperTotalThrustN) || UpperTotalThrustN <= 0) return {};
    const Vec3 Delta = SamplePositionM - UpperPositionM;
    const double Height = -Delta.Z;
    if (Height <= 0) return {};
    const double Width = Positive(Params.CoreRadiusM, 0.18) + Nonnegative(Params.SpreadPerMeter) * Height;
    const double RadiusSquared = Delta.X * Delta.X + Delta.Y * Delta.Y;
    const double Induced = std::sqrt(UpperTotalThrustN / (2.0 * Positive(Params.DensityKgM3, 1.225) * Positive(Params.TotalRotorDiskAreaM2, 0.0380133)));
    const double Attenuation = std::exp(-RadiusSquared / (2.0 * Width * Width) - Height / Positive(Params.VerticalDecayM, 1.4));
    const double Speed = Induced * Attenuation * (Height / (Height + 0.05)) * Nonnegative(Params.Coupling);
    return {0, 0, -ClampFinite(Speed, 0, Positive(Params.MaximumAirSpeedMS, 2))};
}

Vec3 GyroTorque(const Vec3& BodyRateRadS, const std::array<double, 4>& RotorOmegaRadS,
                const std::array<int, 4>& SpinSigns, const GyroParams& Params, bool Enabled)
{
    if (!Enabled || !BodyRateRadS.Finite()) return {};
    double Momentum = 0;
    for (std::size_t Index = 0; Index < 4; ++Index)
        Momentum += Nonnegative(Params.RotorInertiaKgM2) * ClampFinite(RotorOmegaRadS[Index], 0, 10000) * (SpinSigns[Index] < 0 ? -1 : 1);
    return ClampMagnitude(-Cross(BodyRateRadS, Vec3{0, 0, Momentum}), Positive(Params.MaximumTorqueNm, 0.08));
}

SensorPipeline::SensorPipeline(std::uint64_t Seed) { Reset(Seed); }
void SensorPipeline::Reset(std::uint64_t Seed)
{
    RandomState = Seed ? Seed : 1;
    LastInputTime = -1;
    Queue.clear();
    LastOutput = {};
}
double SensorPipeline::Gaussian()
{
    const auto Uniform = [this]()
    {
        RandomState ^= RandomState >> 12; RandomState ^= RandomState << 25; RandomState ^= RandomState >> 27;
        const std::uint64_t Bits = RandomState * UINT64_C(2685821657736338717);
        return (static_cast<double>(Bits >> 11) + 0.5) / 9007199254740992.0;
    };
    const double U = Uniform(), V = Uniform();
    return std::sqrt(-2.0 * std::log(U)) * std::cos(2.0 * Pi * V);
}
SensorReading SensorPipeline::Step(double TimeSeconds, const Vec3& BodyRateRadS, const Vec3& BodySpecificForceMS2, const SensorParams& Params, bool Enabled)
{
    if (!std::isfinite(TimeSeconds) || !BodyRateRadS.Finite() || !BodySpecificForceMS2.Finite())
    {
        Queue.clear(); LastOutput = {}; return LastOutput;
    }
    if (TimeSeconds < LastInputTime) { Queue.clear(); LastOutput = {}; }
    LastInputTime = TimeSeconds;
    SensorReading Reading{TimeSeconds, BodyRateRadS, BodySpecificForceMS2, true};
    if (!Enabled) { Queue.clear(); LastOutput = Reading; return Reading; }
    const double GyroStd = ClampFinite(Params.GyroNoiseStdRadS, 0, 10);
    const double AccelStd = ClampFinite(Params.AccelNoiseStdMS2, 0, 100);
    Reading.GyroRadS += ClampMagnitude(Params.GyroBiasRadS, 10) + Vec3{Gaussian(), Gaussian(), Gaussian()} * GyroStd;
    Reading.SpecificForceMS2 += ClampMagnitude(Params.AccelBiasMS2, 100) + Vec3{Gaussian(), Gaussian(), Gaussian()} * AccelStd;
    Queue.push_back(Reading);
    const double DeliveryTime = TimeSeconds - ClampFinite(Params.LatencySeconds, 0, 0.25);
    while (!Queue.empty() && Queue.front().TimeSeconds <= DeliveryTime + 1e-12)
    {
        LastOutput = Queue.front(); Queue.pop_front();
    }
    while (Queue.size() > 1024) Queue.pop_front();
    return LastOutput;
}

double AntiWindupIntegral(double PreviousIntegral, double Error, double UnsaturatedOutput, double SaturatedOutput,
                          double Ki, double Dt, double BackCalculationGain, double MaximumAbsIntegral)
{
    const double Limit = Positive(MaximumAbsIntegral, 0.5);
    double Integral = ClampFinite(PreviousIntegral, -Limit, Limit);
    if (!std::isfinite(Error) || !std::isfinite(UnsaturatedOutput) || !std::isfinite(SaturatedOutput) || !std::isfinite(Dt) || Dt <= 0) return Integral;
    if (!std::isfinite(Ki) || Ki <= 1e-12) return 0;
    // Integrate only if unsaturated, or when error moves demand out of saturation.
    if ((UnsaturatedOutput - SaturatedOutput) * Error <= 0) Integral += Error * Dt;
    Integral += Nonnegative(BackCalculationGain) * (SaturatedOutput - UnsaturatedOutput) * Dt / Ki;
    return ClampFinite(Integral, -Limit, Limit);
}

double PidStep(PidState& State, double Error, double MeasuredDerivative, double Dt,
                double Kp, double Ki, double Kd, double MinimumOutput, double MaximumOutput,
                double BackCalculationGain, double IntegralLimit)
{
    if (!std::isfinite(Error) || !std::isfinite(MeasuredDerivative) || !std::isfinite(MinimumOutput) ||
        !std::isfinite(MaximumOutput) || MaximumOutput < MinimumOutput) return 0;
    const double Unsaturated = Nonnegative(Kp) * Error + Nonnegative(Ki) * State.Integral - Nonnegative(Kd) * MeasuredDerivative;
    const double Output = ClampFinite(Unsaturated, MinimumOutput, MaximumOutput);
    State.Integral = AntiWindupIntegral(State.Integral, Error, Unsaturated, Output, Ki, Dt, BackCalculationGain, IntegralLimit);
    return Output;
}

double SpringDamper(double PenetrationM, double NormalVelocityMS, const ContactParams& Params, bool Enabled)
{
    if (!Enabled || !std::isfinite(PenetrationM) || !std::isfinite(NormalVelocityMS) || PenetrationM <= 0) return 0;
    const double Force = Nonnegative(Params.StiffnessNPerM) * PenetrationM - Nonnegative(Params.DampingNsPerM) * NormalVelocityMS;
    return ClampFinite(Force, 0, Positive(Params.MaximumForceN, 25));
}

double NetContactStep(NetState& State, double PenetrationM, double NormalVelocityMS, double Dt, const ContactParams& Params, bool Enabled)
{
    if (!Enabled) { State = {}; return 0; }
    const double Force = SpringDamper(PenetrationM, NormalVelocityMS, Params);
    const double MaximumDeflection = Positive(Params.MaximumNetDeflectionM, 0.2);
    const double MaximumCompression = ClampFinite(Params.MaximumCageCompression, 0, 0.2, 0.08);
    const double Deflection = ClampFinite(PenetrationM, 0, MaximumDeflection);
    const double Compression = MaximumCompression * Force / Positive(Params.MaximumForceN, 25);
    State.DeflectionM = ClampFinite(State.DeflectionM, 0, MaximumDeflection);
    State.CageCompression = ClampFinite(State.CageCompression, 0, MaximumCompression);
    const double Alpha = Response(Dt, Params.NetResponseSeconds);
    State.DeflectionM += (Deflection - State.DeflectionM) * Alpha;
    State.CageCompression += (Compression - State.CageCompression) * Alpha;
    return Force;
}

bool InertiaTensor::PositiveDefinite() const
{
    const double Minor = Ixx * Iyy - Ixy * Ixy;
    const double Determinant = Ixx * (Iyy * Izz - Iyz * Iyz) - Ixy * (Ixy * Izz - Iyz * Ixz) + Ixz * (Ixy * Iyz - Iyy * Ixz);
    return std::isfinite(Ixx) && std::isfinite(Iyy) && std::isfinite(Izz) && std::isfinite(Ixy) &&
           std::isfinite(Ixz) && std::isfinite(Iyz) && Ixx > 0 && Minor > 0 && Determinant > 0;
}
InertiaTensor ComputeInertia(const std::vector<MassPoint>& Components, const Vec3& CenterOfMassM, double MinimumInertiaKgM2)
{
    InertiaTensor Tensor;
    const Vec3 Center = CenterOfMassM.Finite() ? CenterOfMassM : Vec3{};
    for (const MassPoint& Component : Components)
    {
        if (!Component.PositionM.Finite() || !std::isfinite(Component.MassKg) || Component.MassKg <= 0) continue;
        const double Mass = std::min(Component.MassKg, 1000.0);
        const Vec3 R = ClampMagnitude(Component.PositionM - Center, 1000);
        Tensor.Ixx += ClampFinite(Component.IntrinsicInertiaKgM2.X, 0, 1000000.0) + Mass * (R.Y * R.Y + R.Z * R.Z);
        Tensor.Iyy += ClampFinite(Component.IntrinsicInertiaKgM2.Y, 0, 1000000.0) + Mass * (R.X * R.X + R.Z * R.Z);
        Tensor.Izz += ClampFinite(Component.IntrinsicInertiaKgM2.Z, 0, 1000000.0) + Mass * (R.X * R.X + R.Y * R.Y);
        Tensor.Ixy -= Mass * R.X * R.Y; Tensor.Ixz -= Mass * R.X * R.Z; Tensor.Iyz -= Mass * R.Y * R.Z;
    }
    const double Floor = ClampFinite(MinimumInertiaKgM2, 1e-9, 1, 1e-6);
    Tensor.Ixx += Floor; Tensor.Iyy += Floor; Tensor.Izz += Floor;
    return Tensor;
}
Vec3 InertiaDiagonal(const std::vector<MassPoint>& Components, const Vec3& CenterOfMassM, double MinimumInertiaKgM2)
{
    return ComputeInertia(Components, CenterOfMassM, MinimumInertiaKgM2).Diagonal();
}
}
