#pragma once
#include <algorithm>
#include <cmath>

// SI impact-work approximation. Coefficients are tunable simulator assumptions,
// not measured cage fatigue or a prediction of real structural failure.
namespace ImperialContact
{
struct GuardParameters
{
    double DamageThresholdJ=.6;
    double EnergyCapacityJ=80;
    double MaximumLossPerHit=.25;
};
struct ImpactResult {double DissipatedJ=0,IntegrityLoss=0;bool Significant=false;};
struct GuardState
{
    double Integrity=1;
    double TotalDissipatedJ=0;
    unsigned ImpactCount=0;
    ImpactResult Apply(double ImpulseNs,double BodyMassKg,double OtherMassKg,double Restitution,
                       double EnergyShare,const GuardParameters& P)
    {
        ImpactResult R;
        if(!std::isfinite(ImpulseNs)||ImpulseNs<=0||!std::isfinite(BodyMassKg)||BodyMassKg<=0)return R;
        const double J=std::min(ImpulseNs,1000.);
        const double InvOther=std::isfinite(OtherMassKg)&&OtherMassKg>0?1./OtherMassKg:0.;
        const double ReducedMass=1./(1./BodyMassKg+InvOther);
        const double E=std::isfinite(Restitution)?std::clamp(Restitution,0.,1.):.25;
        const double Share=std::isfinite(EnergyShare)?std::clamp(EnergyShare,0.,1.):0;
        R.DissipatedJ=std::min(10000.,J*J/(2.*ReducedMass*(1.+E)*(1.+E))*(1.-E*E))*Share;
        const double Threshold=std::isfinite(P.DamageThresholdJ)?std::max(0.,P.DamageThresholdJ):.6;
        if(R.DissipatedJ<=Threshold)return R;
        const double Capacity=std::isfinite(P.EnergyCapacityJ)?std::max(1.,P.EnergyCapacityJ):80.;
        const double MaxLoss=std::isfinite(P.MaximumLossPerHit)?std::clamp(P.MaximumLossPerHit,0.,1.):.25;
        const double Before=std::isfinite(Integrity)?std::clamp(Integrity,0.,1.):0.;
        R.IntegrityLoss=std::min(Before,std::min(MaxLoss,(R.DissipatedJ-Threshold)/Capacity));
        Integrity=Before-R.IntegrityLoss;TotalDissipatedJ+=R.DissipatedJ;++ImpactCount;R.Significant=true;
        return R;
    }
};
}
