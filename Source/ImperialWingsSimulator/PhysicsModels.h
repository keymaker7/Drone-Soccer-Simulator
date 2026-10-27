#pragma once

// Engine-independent SI models. Every hardware coefficient below is a configurable
// prototype assumption; none is a measured Boxer/airframe/ESC/battery specification.
#include <array>
#include <cstdint>
#include <deque>
#include <vector>

namespace ImperialPhysics
{
struct Vec3
{
    double X = 0, Y = 0, Z = 0;
    Vec3() = default;
    Vec3(double XValue, double YValue, double ZValue) : X(XValue), Y(YValue), Z(ZValue) {}
    Vec3 operator+(const Vec3& V) const { return {X + V.X, Y + V.Y, Z + V.Z}; }
    Vec3 operator-(const Vec3& V) const { return {X - V.X, Y - V.Y, Z - V.Z}; }
    Vec3 operator-() const { return {-X, -Y, -Z}; }
    Vec3 operator*(double S) const { return {X * S, Y * S, Z * S}; }
    Vec3 operator/(double S) const;
    Vec3& operator+=(const Vec3& V) { X += V.X; Y += V.Y; Z += V.Z; return *this; }
    double LengthSquared() const;
    double Length() const;
    bool Finite() const;
};
Vec3 operator*(double S, const Vec3& V);
double Dot(const Vec3& A, const Vec3& B);
Vec3 Cross(const Vec3& A, const Vec3& B);
Vec3 ClampMagnitude(const Vec3& V, double Maximum);
double ClampFinite(double Value, double Minimum, double Maximum, double Fallback = 0);
// Stick commands set a bounded tilt cone in a horizontal heading frame. Yaw
// remains a separate rate command; neither helper holds position or altitude.
Vec3 AngleDesiredUp(double Roll, double Pitch, double MaximumTiltDegrees = 35);
Vec3 AngleBodyRate(const Vec3& DesiredUpInBody, double GainPerSecond = 4,
                   double MaximumRateRadS = 3.141592653589793);

struct MotorParams
{
    double MaxThrustN = 8.0;
    double IdleThrustN = 0.12;
    double ResponseSeconds = 0.035;
    double PwmExponent = 2.0;
    double MaximumOmegaRadS = 1800.0;
    double RotorRadiusM = 0.055;
    double ElectricalEfficiency = 0.32; // Lumped effective motor/profile loss assumption.
};
struct MotorState { double ThrustN = 0, OmegaRadS = 0; };
// PWM 0 stops the motor; positive PWM includes configured armed idle thrust.
double MotorStep(MotorState& State, double Pwm, double Dt, const MotorParams& Params,
                 double VoltageFactor = 1.0, bool Enabled = true);
double PwmForThrust(double DesiredThrustN, const MotorParams& Params,
                    double VoltageFactor = 1.0, bool Nonlinear = true);
double EstimateRotorPowerW(double ThrustN, const MotorParams& Params, double DensityKgM3 = 1.225);

struct BatteryParams
{
    double NominalVoltageV = 14.8, FullVoltageV = 16.8, EmptyVoltageV = 13.2;
    double CapacityAh = 1.5, InternalResistanceOhm = 0.035, MaximumCurrentA = 80.0;
    double AvionicsCurrentA = 0.3;
};
struct BatteryState
{
    double Soc = 1.0, VoltageV = 16.8, CurrentA = 0, ConsumedAh = 0, DeliveredPowerW = 0;
    bool PowerLimited = false;
};
void BatteryStep(BatteryState& State, double ElectricalPowerW, double Dt,
                 const BatteryParams& Params, bool Enabled = true);
double BatteryThrustFactor(const BatteryState& State, const BatteryParams& Params, bool Enabled = true);

struct PowerBudgetResult
{
    double RequestedMotorPowerW = 0;
    double AppliedMotorPowerW = 0;
    double AvailableMotorPowerW = 0;
    double ThrustScale = 1;
    bool Limited = false;
};
// Allocate candidate (post-ESC) motor states against this step's actual terminal
// battery power. Advances Battery exactly once for a finite positive Dt. Electrical
// power is proportional to T^(3/2), so a common T scale preserves mixer proportions.
// Thrust and rotor omega are updated together before any force/gyro/wake use.
// Invalid Dt rejects output (zero motors) without advancing battery charge.
// Disabled battery leaves valid motor candidates unchanged and does not drain SOC.
PowerBudgetResult ApplyBatteryPowerBudget(std::array<MotorState,4>& Motors, BatteryState& Battery,
    double Dt, const MotorParams& Motor, const BatteryParams& BatteryParameters,
    double DensityKgM3 = 1.225, bool Enabled = true);

struct AeroParams { double DensityKgM3 = 1.225, EffectiveCdAreaM2 = 0.015; };
// Relative velocity is aircraft velocity minus ambient/wake air velocity.
// Effective Cd*A is configurable and does not assume a solid sphere drag coefficient.
Vec3 DragForce(const Vec3& RelativeAirVelocityMS, const AeroParams& Params, bool Enabled = true);

struct SurfaceParams
{
    double RotorRadiusM = 0.055, InfluenceRotorRadii = 4.0;
    double GroundGain = 0.08, CeilingGain = 0.03, WallLoss = 0.025;
    double MinimumFactor = 0.85, MaximumFactor = 1.12;
};
// Heuristic bounded proximity correction. Distances are rotor-plane clearances in m;
// pass infinity when a surface is absent. This is not CFD or a calibrated wall model.
double SurfaceLiftFactor(double GroundDistanceM, double CeilingDistanceM, double WallDistanceM,
                         const SurfaceParams& Params, bool Enabled = true);

struct DownwashParams
{
    double DensityKgM3 = 1.225, TotalRotorDiskAreaM2 = 0.0380133;
    double CoreRadiusM = 0.18, SpreadPerMeter = 0.3, VerticalDecayM = 1.4;
    double Coupling = 0.18, MaximumAirSpeedMS = 2.0;
};
Vec3 DownwashVelocity(const Vec3& UpperPositionM, const Vec3& SamplePositionM,
                       double UpperTotalThrustN, const DownwashParams& Params, bool Enabled = true);
// Body-frame rotor angular momentum; signs normally {+1,-1,+1,-1}.
struct GyroParams { double RotorInertiaKgM2 = 0.0000035, MaximumTorqueNm = 0.08; };
Vec3 GyroTorque(const Vec3& BodyRateRadS, const std::array<double, 4>& RotorOmegaRadS,
                const std::array<int, 4>& SpinSigns, const GyroParams& Params, bool Enabled = true);

struct SensorParams
{
    double GyroNoiseStdRadS = 0.0005, AccelNoiseStdMS2 = 0.005, LatencySeconds = 0.005;
    Vec3 GyroBiasRadS{0.0002, -0.0001, 0.0001};
    Vec3 AccelBiasMS2{0.005, -0.005, 0.008};
};
struct SensorReading
{
    double TimeSeconds = 0;
    Vec3 GyroRadS, SpecificForceMS2; // Accelerometer measures a-g, in body coordinates.
    bool Valid = false;
};
class SensorPipeline
{
public:
    explicit SensorPipeline(std::uint64_t Seed = 1);
    void Reset(std::uint64_t Seed = 1);
    SensorReading Step(double TimeSeconds, const Vec3& BodyRateRadS, const Vec3& BodySpecificForceMS2,
                        const SensorParams& Params, bool Enabled = true);
private:
    std::uint64_t RandomState = 1;
    double LastInputTime = -1;
    std::deque<SensorReading> Queue;
    SensorReading LastOutput;
    double Gaussian();
};

double AntiWindupIntegral(double PreviousIntegral, double Error, double UnsaturatedOutput,
                          double SaturatedOutput, double Ki, double Dt,
                          double BackCalculationGain = 2.0, double MaximumAbsIntegral = 0.5);
struct PidState { double Integral = 0; };
double PidStep(PidState& State, double Error, double MeasuredDerivative, double Dt,
                double Kp, double Ki, double Kd, double MinimumOutput, double MaximumOutput,
                double BackCalculationGain = 2.0, double IntegralLimit = 0.5);

struct ContactParams
{
    double StiffnessNPerM = 60.0, DampingNsPerM = 9.0, MaximumForceN = 25.0;
    double MaximumNetDeflectionM = 0.2, NetResponseSeconds = 0.08;
    double MaximumCageCompression = 0.08; // Visual fraction; never changes collision radius.
};
// Positive normal velocity points out of the contact. Result is a nonnegative
// outward force magnitude, with no adhesive force when separating.
double SpringDamper(double PenetrationM, double NormalVelocityMS, const ContactParams& Params, bool Enabled = true);
struct NetState { double DeflectionM = 0, CageCompression = 0; };
double NetContactStep(NetState& State, double PenetrationM, double NormalVelocityMS,
                       double Dt, const ContactParams& Params, bool Enabled = true);

struct MassPoint
{
    double MassKg = 0;
    Vec3 PositionM;
    Vec3 IntrinsicInertiaKgM2; // Local diagonal about the component's own center.
};
struct InertiaTensor
{
    double Ixx = 0, Iyy = 0, Izz = 0, Ixy = 0, Ixz = 0, Iyz = 0;
    Vec3 Diagonal() const { return {Ixx, Iyy, Izz}; }
    bool PositiveDefinite() const;
};
InertiaTensor ComputeInertia(const std::vector<MassPoint>& Components, const Vec3& CenterOfMassM = {},
                              double MinimumInertiaKgM2 = 0.000001);
Vec3 InertiaDiagonal(const std::vector<MassPoint>& Components, const Vec3& CenterOfMassM = {},
                      double MinimumInertiaKgM2 = 0.000001);

struct Parameters
{
    MotorParams Motor;
    BatteryParams Battery;
    AeroParams Aero;
    SurfaceParams Surface;
    DownwashParams Downwash;
    GyroParams Gyro;
    SensorParams Sensors;
    ContactParams Contact;
};
}
