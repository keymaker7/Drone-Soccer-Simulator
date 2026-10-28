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
struct GoalFrame
{
    Vec3 center{0,0,3},normal{1,0,0},side{0,1,0},up{0,0,1};
};
std::array<GoalFrame,2> NominalGoalFrames();

class MatchRules
{
public:
    MatchRules();
    void Reset();
    void StartMatch();
    void StartPractice();
    void SetActive(int id, bool active);
    void Update(const std::array<Vec3,10>& positions, double dt);
    void Update(const std::array<Vec3,10>& positions,double dt,const std::array<GoalFrame,2>& goals);
    int Score(int team) const;
    bool IsOffside(int team) const;
    int Penalties(int team) const;
    int SetWins(int team) const;
    // Current scoring Striker per team; -1 for an invalid team. IDs follow the
    // pawn roles: team*5 is the original Striker and team*5+1 is the Guide.
    int Striker(int team) const;
    bool IsStriker(int id) const;
    // Records the referee's confirmation that the Guide has taken over the
    // Striker role. Returns false when the call is not permitted.
    bool ConfirmStrikerSuccession(int team);
    int CurrentSet() const { return currentSet; }
    double Remaining() const { return remaining; }
    Phase GetPhase() const { return phase; }
    bool IsArmingAllowed() const { return phase==Phase::Practice || phase==Phase::Playing; }
private:
    struct Transit { int direction=0; bool clear=true; };
    std::array<Vec3,10> previous{};
    std::array<GoalFrame,2> previousGoals{};
    std::array<bool,10> active{};
    std::array<std::array<Transit,2>,10> transits{};
    std::array<int,2> scores{}, penalties{}, wins{}, strikers{};
    std::array<bool,2> offside{};
    Phase phase=Phase::Practice;
    int currentSet=0;
    double remaining=0;
    bool havePrevious=false;
    void AdvanceClock(double dt);
    void ProcessMotion(int id, int attackingTeam, Vec3 from, Vec3 to,bool bodyMoved=true);
    void ClearTransits();
};
}
