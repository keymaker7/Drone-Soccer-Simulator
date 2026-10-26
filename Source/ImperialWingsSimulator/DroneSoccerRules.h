#pragma once
#include <array>

// Portable FIDA Class 40 rule profile. Positions are metres, time is seconds.
namespace drone_soccer
{
struct Vec3 { double x=0, y=0, z=0; };
struct RuleProfileFIDA40
{
    static constexpr double Length=16.0, Width=7.0, Height=4.5;
    static constexpr double GoalX=6.5, GoalHeight=3.0;
    static constexpr double GoalInnerRadius=0.30, GoalOuterRadius=0.50;
    static constexpr double BallRadius=0.20, MassMax=1.10;
    // Axial mesh thickness is a simulator choice; the FIDA web text does not specify it.
    static constexpr double GoalThickness=0.16;
    static constexpr double ReadySeconds=10.0, SetSeconds=180.0, BreakSeconds=300.0;
};
enum class Phase { Practice, Ready, Playing, Break, Finished };

class MatchRules
{
public:
    MatchRules();
    void Reset();
    void StartMatch();
    void StartPractice();
    void SetActive(int id, bool active);
    void Update(const std::array<Vec3,10>& positions, double dt);
    int Score(int team) const;
    bool IsOffside(int team) const;
    int Penalties(int team) const;
    int SetWins(int team) const;
    int CurrentSet() const { return currentSet; }
    double Remaining() const { return remaining; }
    Phase GetPhase() const { return phase; }
    bool IsArmingAllowed() const { return phase==Phase::Practice || phase==Phase::Playing; }
private:
    struct Transit { int direction=0; bool clear=true; };
    std::array<Vec3,10> previous{};
    std::array<bool,10> active{};
    std::array<std::array<Transit,2>,10> transits{};
    std::array<int,2> scores{}, penalties{}, wins{};
    std::array<bool,2> offside{};
    Phase phase=Phase::Practice;
    int currentSet=0;
    double remaining=0;
    bool havePrevious=false;
    void AdvanceClock(double dt);
    void ProcessMotion(int id, int attackingTeam, Vec3 from, Vec3 to);
    void ClearTransits();
};
}
