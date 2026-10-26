// Deterministic internal SI fixtures. No aircraft specifications are measured here.
#include "PhysicsModels.h"
#include "ContactResponse.h"
#include <cmath>
#include <iostream>
#include <limits>

using namespace ImperialPhysics;
namespace
{
int Checks = 0, Failures = 0;
void Check(bool Value, const char* Name) { ++Checks; if (!Value) { ++Failures; std::cerr << "FAIL: " << Name << '\n'; } }
bool Near(double A, double B, double Tolerance = 1e-9) { return std::abs(A - B) <= Tolerance; }
}

int main()
{
    Parameters P;
    const double NaN = std::numeric_limits<double>::quiet_NaN();
    ImperialContact::GuardParameters GuardP;ImperialContact::GuardState Guard;
    for(int I=0;I<10000;++I)Guard.Apply(.08,.9,0,.25,1,GuardP);
    Check(Near(Guard.Integrity,1)&&Guard.ImpactCount==0,"resting contact cannot wear out the cage");
    const auto Impact=Guard.Apply(4,.9,0,.25,1,GuardP);
    Check(Impact.Significant&&Impact.IntegrityLoss>0&&Impact.IntegrityLoss<.25,"actual impact work reduces bounded cage integrity");
    const double AfterFirst=Guard.Integrity;Guard.Apply(4,.9,0,.25,1,GuardP);
    Check(Guard.Integrity<AfterFirst&&Guard.ImpactCount==2,"successive impacts accumulate without automatic repair");
    ImperialContact::GuardState Elastic;Elastic.Apply(4,.9,0,1,1,GuardP);
    Check(Near(Elastic.Integrity,1),"ideal elastic collision has no dissipated impact-work wear");
    ImperialContact::GuardState GuardA,GuardB,Whole;
    const auto RA=GuardA.Apply(4,.9,.9,.25,.5,GuardP);const auto RB=GuardB.Apply(4,.9,.9,.25,.5,GuardP);const auto RW=Whole.Apply(4,.9,.9,.25,1,GuardP);
    Check(Near(RA.DissipatedJ+RB.DissipatedJ,RW.DissipatedJ),"two cage contacts share dissipation rather than count it twice");
    Check(!Guard.Apply(NaN,.9,0,.25,1,GuardP).Significant&&!Guard.Apply(-1,.9,0,.25,1,GuardP).Significant,"invalid or negative impulses cannot generate wear");
    Check(!Guard.Apply(3,0,0,.25,1,GuardP).Significant,"zero mass cannot create invalid impact energy");
    for(int I=0;I<30;++I)Guard.Apply(100,.9,0,.25,1,GuardP);
    Check(Near(Guard.Integrity,0)&&std::isfinite(Guard.TotalDissipatedJ),"severe repeated impacts saturate at zero integrity");
    Check(!Vec3{NaN, 0, 0}.Finite(), "non-finite vector is identified");
    Check(Near(Vec3{1, 2, 2}.Length(), 3), "SI vector magnitude");
    Check(Near(Dot(Cross({1,0,0}, {0,1,0}), {0,0,1}), 1), "right handed vector cross product");
    Check(Near((Vec3{1,2,3} / 0).Length(), 0), "zero division returns finite zero");
    Check(Near(ClampMagnitude({3,4,0}, 2).Length(), 2), "vector clamp preserves bounded magnitude");
    Check(Near(ClampFinite(NaN, -1, 1), 0), "invalid scalar returns a finite fallback");
    Check(Near(AngleBodyRate(AngleDesiredUp(0,0)).Length(),0),"centered angle input holds a level attitude");
    Check(AngleBodyRate(AngleDesiredUp(1,0)).X<0,"right roll commands rightward body tilt");
    Check(AngleBodyRate(AngleDesiredUp(0,1)).Y>0,"forward pitch commands forward body tilt");
    const auto DiagonalTilt=AngleDesiredUp(1,1);
    Check(Near(DiagonalTilt.Length(),1)&&Near(std::acos(DiagonalTilt.Z)*180/3.141592653589793,35),"diagonal stick input respects total tilt cone limit");
    Check(AngleBodyRate({0,0,-1}).Finite()&&AngleBodyRate({0,0,-1}).Length()>0,"inverted aircraft receives bounded self-level recovery");
    Check(AngleBodyRate({0,0,-1},30,1).Length()<=1.000001,"angle controller rate is limited independently from stick angle");
    Check(Near(AngleBodyRate({NaN,0,1}).Length(),0),"invalid attitude cannot create a rate command");
    Check(Near(AngleDesiredUp(NaN,NaN).Z,1),"invalid sticks fall back to level attitude");
    for(int Rate:{20,30,60,120,240})
    {
        double Tilt=.5;
        for(int I=0;I<Rate*2;++I)Tilt-=AngleBodyRate({0,-std::sin(Tilt),std::cos(Tilt)}).X/Rate;
        Check(std::abs(Tilt)<.002,"released angle input converges to level across controller sample intervals");
    }

    MotorState Motor;
    const double AtTau = MotorStep(Motor, 1, P.Motor.ResponseSeconds, P.Motor);
    Check(Near(AtTau, P.Motor.MaxThrustN * (1 - std::exp(-1.0))), "ESC step reaches analytical 63.2 percent at one time constant");
    MotorState TwoHalfSteps;
    MotorStep(TwoHalfSteps, 1, P.Motor.ResponseSeconds * 0.5, P.Motor);
    MotorStep(TwoHalfSteps, 1, P.Motor.ResponseSeconds * 0.5, P.Motor);
    Check(Near(Motor.ThrustN, TwoHalfSteps.ThrustN), "constant-input ESC response is independent of step subdivision");
    MotorState Nonlinear, Linear;
    MotorStep(Nonlinear, 0.5, 2, P.Motor);
    MotorStep(Linear, 0.5, 2, P.Motor, 1, false);
    Check(Nonlinear.ThrustN < Linear.ThrustN && Near(Linear.ThrustN, 0.12 + 7.88 * 0.5), "nonlinear PWM differs from optional linear baseline");
    bool InverseCorrect = true;
    for (double Factor : {0.7, 1.0, 1.25})
    {
        for (double Thrust : {0.0, 0.12, 1.0, 2.20725, 4.0})
        {
            MotorState State;
            const double Command = PwmForThrust(Thrust, P.Motor, Factor);
            MotorStep(State, Command, 3, P.Motor, Factor);
            const double Attainable = Thrust <= 0 ? 0 : std::max(P.Motor.IdleThrustN * Factor, Thrust);
            if (!Near(State.ThrustN, Attainable, 1e-7)) InverseCorrect = false;
        }
    }
    Check(InverseCorrect, "inverse PWM maintains requested Newton thrust through battery voltage changes");
    MotorState Saturated;
    MotorStep(Saturated, 10, 2, P.Motor, 0.7);
    Check(Near(Saturated.ThrustN, P.Motor.MaxThrustN * 0.7), "ESC and voltage saturation bound maximum thrust");
    const double BeforeStop = Saturated.ThrustN;
    MotorStep(Saturated, 0, P.Motor.ResponseSeconds, P.Motor);
    Check(Saturated.ThrustN < BeforeStop && Saturated.ThrustN >= 0, "motor coasts down after PWM stop");
    Check(Near(PwmForThrust(5, P.Motor, 0), 0), "empty battery cannot request effective PWM");
    MotorState InvalidMotor{NaN, NaN}; MotorStep(InvalidMotor, NaN, 0.01, P.Motor);
    Check(std::isfinite(InvalidMotor.ThrustN) && std::isfinite(InvalidMotor.OmegaRadS), "invalid motor history cannot produce NaN force");
    Check(EstimateRotorPowerW(4, P.Motor) > EstimateRotorPowerW(2, P.Motor) * 2, "induced power grows faster than thrust");

    BatteryParams Ideal = P.Battery;
    Ideal.AvionicsCurrentA = 0; Ideal.InternalResistanceOhm = 0;
    Ideal.FullVoltageV = Ideal.EmptyVoltageV = Ideal.NominalVoltageV;
    BatteryState Charge;
    BatteryStep(Charge, Ideal.NominalVoltageV * 10, 60, Ideal);
    Check(Near(Charge.CurrentA, 10), "ideal battery obeys power equals voltage times current");
    Check(Near(Charge.ConsumedAh, 10.0 / 60), "coulomb counting integrates amp seconds to Ah");
    Check(Near(Charge.Soc, 1 - (10.0 / 60) / Ideal.CapacityAh), "battery SOC matches analytical charge depletion");
    BatteryState Sag;
    BatteryStep(Sag, 250, 0.01, P.Battery);
    Check(Sag.VoltageV < P.Battery.FullVoltageV && Sag.VoltageV > 0 && Sag.CurrentA > 0, "internal resistance causes positive load voltage sag");
    Check(Near(Sag.VoltageV * Sag.CurrentA, 250 + P.Battery.AvionicsCurrentA * P.Battery.FullVoltageV, 1e-7), "battery quadratic solves loaded terminal power");
    BatteryState NoLoad;
    Check(BatteryThrustFactor(Sag, P.Battery) < BatteryThrustFactor(NoLoad, P.Battery), "voltage sag reduces available thrust factor");
    BatteryState LowCharge; LowCharge.Soc = 0.15;
    BatteryStep(LowCharge, 250, 0.01, P.Battery);
    Check(LowCharge.VoltageV < Sag.VoltageV, "same load at low SOC has less voltage");
    BatteryState Overload;
    BatteryStep(Overload, 1e7, 1, P.Battery);
    Check(Overload.PowerLimited && Overload.CurrentA <= P.Battery.MaximumCurrentA && Overload.VoltageV >= 0, "unattainable battery power is current limited without invalid voltage");
    BatteryState Exhausted; Exhausted.Soc = 0;
    BatteryStep(Exhausted, 100, 10, P.Battery);
    Check(Near(BatteryThrustFactor(Exhausted, P.Battery), 0) && Near(Exhausted.CurrentA, 0), "empty battery removes available motor power");
    BatteryState Disabled; Disabled.Soc = 0.5;
    BatteryStep(Disabled, 500, 60, P.Battery, false);
    Check(Near(Disabled.Soc, 0.5) && Near(BatteryThrustFactor(Disabled, P.Battery, false), 1), "disabled battery preserves SOC and nominal baseline thrust");
    BatteryState LongStep;
    BatteryStep(LongStep, Ideal.NominalVoltageV * 10, 3600, Ideal);
    Check(Near(LongStep.Soc, 0) && Near(LongStep.ConsumedAh, Ideal.CapacityAh), "large step cannot consume more than stored battery charge");

    const Vec3 V{3, -4, 2};
    const Vec3 Drag = DragForce(V, P.Aero);
    Check(Dot(Drag, V) < 0, "aerodynamic drag always removes relative kinetic energy");
    Check(Near(DragForce({20,0,0}, P.Aero).Length(), 4 * DragForce({10,0,0}, P.Aero).Length()), "quadratic drag has speed squared scaling");
    Check(Near(DragForce({}, P.Aero).Length(), 0), "co-moving air creates zero relative drag");
    Check(Near(DragForce(V, P.Aero, false).Length(), 0), "drag model can be disabled to baseline");
    double Speed = 5, PreviousEnergy = 0.5 * 0.9 * Speed * Speed;
    bool Dissipative = true;
    constexpr double DragDt = 0.0001;
    for (int Index = 0; Index < 10000; ++Index)
    {
        Speed += DragForce({Speed,0,0}, P.Aero).X / 0.9 * DragDt;
        const double Energy = 0.5 * 0.9 * Speed * Speed;
        if (Energy > PreviousEnergy + 1e-12) Dissipative = false;
        PreviousEnergy = Energy;
    }
    const double DragK = 0.5 * P.Aero.DensityKgM3 * P.Aero.EffectiveCdAreaM2;
    Check(Dissipative, "drag-only integration loses energy monotonically");
    Check(Near(Speed, 5.0 / (1 + DragK / 0.9 * 5.0), 0.0001), "drag-only trajectory matches analytical quadratic velocity decay");

    const double Infinity = std::numeric_limits<double>::infinity();
    Check(Near(SurfaceLiftFactor(Infinity, Infinity, Infinity, P.Surface), 1), "absent surfaces do not change lift");
    Check(SurfaceLiftFactor(0.01, Infinity, Infinity, P.Surface) > 1, "near ground increases bounded lift");
    Check(SurfaceLiftFactor(Infinity, 0.01, Infinity, P.Surface) > 1, "ceiling suction heuristic has configured positive sign");
    Check(SurfaceLiftFactor(Infinity, Infinity, 0.01, P.Surface) < 1, "wall-loss heuristic has configured negative sign");
    const double NearAll = SurfaceLiftFactor(-1, -1, -1, P.Surface);
    Check(NearAll >= P.Surface.MinimumFactor && NearAll <= P.Surface.MaximumFactor, "all close-surface corrections remain bounded");
    Check(Near(SurfaceLiftFactor(0, 0, 0, P.Surface, false), 1), "surface model disabled returns exact baseline lift");

    const Vec3 Wake = DownwashVelocity({0,0,2}, {0,0,1.5}, 9, P.Downwash);
    Check(Wake.Z < 0 && Near(Wake.X, 0) && Near(Wake.Y, 0), "wake flows downward below producing aircraft");
    Check(Near(DownwashVelocity({0,0,2}, {0,0,2.1}, 9, P.Downwash).Length(), 0), "downwash never blows on an aircraft above its source");
    Check(DownwashVelocity({0,0,2}, {2,0,1.5}, 9, P.Downwash).Length() < Wake.Length(), "wake decays outside expanding rotor footprint");
    Check(DownwashVelocity({0,0,20}, {0,0,1}, 9, P.Downwash).Length() < Wake.Length(), "wake decays with vertical separation");
    Check(Wake.Length() <= P.Downwash.MaximumAirSpeedMS, "wake speed is bounded");
    Check(DragForce(-Wake, P.Aero).Z < 0, "wake causes downward aerodynamic force on stationary lower aircraft");
    Check(Near(DownwashVelocity({0,0,2}, {0,0,1}, 0, P.Downwash).Length(), 0), "stopped motors produce no downwash");
    Check(Near(DownwashVelocity({0,0,2}, {0,0,1}, 9, P.Downwash, false).Length(), 0), "wake can be disabled");

    const std::array<int,4> Spins{{1,-1,1,-1}};
    Check(Near(GyroTorque({2,3,4}, {{1000,1000,1000,1000}}, Spins, P.Gyro).Length(), 0), "balanced counter-rotating propellers cancel net rotor gyroscopic momentum");
    const Vec3 Rate{2,3,4};
    const Vec3 Torque = GyroTorque(Rate, {{1200,800,1100,900}}, Spins, P.Gyro);
    Check(Near(Dot(Torque, Rate), 0), "rotor gyroscopic torque does no instantaneous work");
    Check(GyroTorque({2,0,0}, {{1200,800,1000,1000}}, Spins, P.Gyro).Y > 0, "body gyro reaction sign opposes rotor momentum rotation");
    Check(Torque.Length() <= P.Gyro.MaximumTorqueNm, "gyroscopic torque limiter bounds reaction");

    SensorParams Clean = P.Sensors;
    Clean.GyroNoiseStdRadS = Clean.AccelNoiseStdMS2 = 0;
    Clean.GyroBiasRadS = {}; Clean.AccelBiasMS2 = {}; Clean.LatencySeconds = 0.02;
    SensorPipeline Delayed(123);
    Check(!Delayed.Step(0, {0,0,0}, {0,0,9.81}, Clean).Valid, "sensor latency reports no invented startup sample");
    Check(!Delayed.Step(0.01, {1,0,0}, {0,0,9.81}, Clean).Valid, "sensor latency holds until configured delay");
    const SensorReading First = Delayed.Step(0.02, {2,0,0}, {0,0,9.81}, Clean);
    Check(First.Valid && Near(First.TimeSeconds, 0) && Near(First.GyroRadS.X, 0), "FIFO emits the correct delayed timestamp and sample");
    const SensorReading Second = Delayed.Step(0.03, {3,0,0}, {0,0,9.81}, Clean);
    Check(Second.Valid && Near(Second.TimeSeconds, 0.01) && Near(Second.GyroRadS.X, 1), "sensor FIFO preserves sample order");
    SensorPipeline SameA(456), SameB(456), Other(457);
    bool Reproducible = true, Different = false;
    SensorParams Instant = P.Sensors; Instant.LatencySeconds = 0;
    for (int Index = 0; Index < 100; ++Index)
    {
        const auto A = SameA.Step(Index * 0.01, Rate, {}, Instant);
        const auto B = SameB.Step(Index * 0.01, Rate, {}, Instant);
        const auto C = Other.Step(Index * 0.01, Rate, {}, Instant);
        if (!Near((A.GyroRadS - B.GyroRadS).Length(), 0, 0)) Reproducible = false;
        if ((A.GyroRadS - C.GyroRadS).Length() > 1e-12) Different = true;
    }
    Check(Reproducible && Different, "seeded sensor noise is repeatable and seed-dependent");
    const auto Bypass = SameA.Step(2, Rate, {0,0,9.81}, P.Sensors, false);
    Check(Bypass.Valid && Near((Bypass.GyroRadS - Rate).Length(), 0) && Near(Bypass.SpecificForceMS2.Z, 9.81), "disabled sensors bypass noise, bias and latency");
    Check(!SameA.Step(3, {NaN,0,0}, {}, P.Sensors).Valid, "invalid sensor source clears its delayed state");
    SensorPipeline FreefallSensor;
    Check(Near(FreefallSensor.Step(0, {}, {}, Clean, false).SpecificForceMS2.Length(), 0), "ideal accelerometer reads zero specific force during free fall");

    double Z = 0, VerticalVelocity = 0;
    constexpr double FreefallDt = 0.005;
    for (int Index = 0; Index < 400; ++Index)
    {
        const Vec3 Force = DragForce({0,0,VerticalVelocity}, P.Aero, false);
        const double A = -9.81 + Force.Z / 0.9;
        Z += VerticalVelocity * FreefallDt + 0.5 * A * FreefallDt * FreefallDt;
        VerticalVelocity += A * FreefallDt;
    }
    Check(Near(Z, -0.5 * 9.81 * 4, 1e-9) && Near(VerticalVelocity, -19.62, 1e-9), "disabled optional forces reproduce analytical two-second free fall");

    Check(AntiWindupIntegral(0.4, 10, 4, 1, 1, 0.1) < 0.4, "back calculation unwinds integral at output saturation");
    Check(Near(AntiWindupIntegral(0.1, 0.2, 0.5, 0.5, 1, 0.1), 0.12), "unsaturated integrator follows error integral");
    Check(Near(AntiWindupIntegral(0.4, 1, 2, 1, 0, 0.1), 0), "zero Ki avoids back-calculation division");
    PidState Controller;
    for (int Index = 0; Index < 10000; ++Index) PidStep(Controller, 100, 0, 0.01, 1, 1, 0, -1, 1);
    Check(std::abs(Controller.Integral) <= 0.5, "persistent saturation cannot wind integral beyond configured bound");
    Check(std::isfinite(PidStep(Controller, NaN, 0, 0.01, 1, 1, 0, -1, 1)), "invalid PID input returns finite force command");

    const double StaticContact = SpringDamper(0.1, 0, P.Contact);
    Check(StaticContact > 0 && SpringDamper(0.1, -1, P.Contact) > StaticContact, "contact damper opposes inward motion");
    Check(SpringDamper(0.1, 1, P.Contact) < StaticContact, "separating velocity reduces contact force");
    Check(Near(SpringDamper(0.1, 100, P.Contact), 0) && Near(SpringDamper(-0.1, -100, P.Contact), 0), "spring contact has no attraction or force outside contact");
    Check(SpringDamper(1, -100, P.Contact) <= P.Contact.MaximumForceN, "contact force remains bounded");
    NetState Net;
    NetContactStep(Net, 0.2, -1, P.Contact.NetResponseSeconds, P.Contact);
    Check(Near(Net.DeflectionM, 0.2 * (1 - std::exp(-1.0))), "net visual deflection follows analytical first-order lag");
    const double LoadedDeflection = Net.DeflectionM;
    NetContactStep(Net, 0, 0, 0.1, P.Contact);
    Check(Net.DeflectionM < LoadedDeflection && Net.CageCompression >= 0 && Net.CageCompression <= P.Contact.MaximumCageCompression, "net and cage visual states recover without overshoot");
    NetContactStep(Net, 0.2, -1, 0.1, P.Contact, false);
    Check(Near(Net.DeflectionM, 0) && Near(Net.CageCompression, 0), "disabled net/contact model restores baseline geometry");
    double X1 = -0.05, X2 = 0.05, V1 = 1, V2 = -1;
    const double InitialEnergy = 0.5 * 0.9 * (V1 * V1 + V2 * V2) + 0.5 * P.Contact.StiffnessNPerM * 0.1 * 0.1;
    for (int Index = 0; Index < 20000; ++Index)
    {
        const double F = SpringDamper(0.2 - (X2 - X1), V2 - V1, P.Contact);
        V1 -= F / 0.9 * 0.0001; V2 += F / 0.9 * 0.0001;
        X1 += V1 * 0.0001; X2 += V2 * 0.0001;
    }
    Check(Near(0.9 * (V1 + V2), 0, 1e-10), "equal opposite contact forces conserve two-body linear momentum");
    Check(0.5 * 0.9 * (V1 * V1 + V2 * V2) < InitialEnergy, "damped contact dissipates impact energy");

    const std::vector<MassPoint> Pair{{1, {-0.1,0,0}, {}}, {1, {0.1,0,0}, {}}};
    const InertiaTensor Tensor = ComputeInertia(Pair);
    Check(Tensor.PositiveDefinite() && Near(Tensor.Iyy, 0.020001) && Near(Tensor.Izz, 0.020001), "parallel-axis inertia matches analytical two-mass result");
    const std::vector<MassPoint> Shifted{{1, {0.9,2,3}, {}}, {1, {1.1,2,3}, {}}};
    const InertiaTensor ShiftedTensor = ComputeInertia(Shifted, {1,2,3});
    Check(Near(Tensor.Ixx, ShiftedTensor.Ixx) && Near(Tensor.Iyy, ShiftedTensor.Iyy) && Near(Tensor.Izz, ShiftedTensor.Izz), "inertia is invariant to joint translation of components and COM");
    Check(ComputeInertia({}).PositiveDefinite(), "empty or degenerate mass model receives a positive inertia floor");
    const InertiaTensor OffAxis = ComputeInertia({{0.2, {0.1,0.2,0.3}, {0.001,0.002,0.003}}});
    Check(OffAxis.PositiveDefinite() && OffAxis.Ixy < 0 && OffAxis.Ixz < 0 && OffAxis.Iyz < 0, "off-diagonal inertia signs follow parallel-axis theorem");
    Check(ComputeInertia({{-1, {1,2,3}, {}}, {1, {NaN,0,0}, {}}}).PositiveDefinite(), "invalid mass points cannot create nonphysical inertia");

    const double Huge = std::numeric_limits<double>::max();
    AeroParams HugeAero{Huge, Huge};
    Check(DragForce({100,200,300}, HugeAero).Finite(), "extreme finite aero parameters cannot overflow force");
    SurfaceParams HugeSurface; HugeSurface.GroundGain = HugeSurface.CeilingGain = HugeSurface.WallLoss = Huge;
    Check(std::isfinite(SurfaceLiftFactor(0, 0, 0, HugeSurface)), "extreme surface gains cannot produce infinity minus infinity");
    MotorParams HugeMotor = P.Motor; HugeMotor.MaxThrustN = Huge; HugeMotor.MaximumOmegaRadS = Huge;
    MotorState HugeMotorState;
    MotorStep(HugeMotorState, 1, 1, HugeMotor);
    Check(std::isfinite(HugeMotorState.ThrustN) && std::isfinite(HugeMotorState.OmegaRadS), "extreme motor parameters stay finite");
    MotorParams TinyRotor = P.Motor; TinyRotor.RotorRadiusM = 1e-300;
    Check(std::isfinite(EstimateRotorPowerW(1, TinyRotor, 1e-300)), "tiny rotor and air-density inputs cannot divide power by zero");
    Check(ComputeInertia({{1, {0.1,0.2,0.3}, {Huge,Huge,Huge}}}).PositiveDefinite(), "extreme intrinsic inertia is bounded to a finite positive tensor");

    std::cout << "Physics model fixtures: " << Checks << " checks, " << Failures << " failures. Coefficients are experimental; no hardware calibration performed.\n";
    return Failures == 0 ? 0 : 1;
}
