#pragma once

#include <cmath>
#include <cstdint>

namespace ImperialGuidance
{
constexpr int ProtocolVersion = 2;
struct Ticket
{
    std::uint64_t Command = 0;
    std::uint64_t Request = 0;
    double SentAt = 0;
};

/** Correlates asynchronous callbacks without letting an old callback release a
 * newer request. Session and JSON validation belong to the transport adapter. */
class RequestWindow
{
public:
    void NewCommand() { ++Generation; Waiting = false; }
    bool Begin(double Now, Ticket& Output)
    {
        if (Waiting || !std::isfinite(Now) || Now < 0) return false;
        Pending = {Generation, ++Sequence, Now};
        Waiting = true;
        Output = Pending;
        return true;
    }
    bool IsCurrent(const Ticket& Value) const
    {
        return Waiting && Value.Command == Generation && Value.Command == Pending.Command && Value.Request == Pending.Request;
    }
    bool Complete(const Ticket& Value, double Now, double MaxAge = 1.0)
    {
        if (!IsCurrent(Value)) return false;
        Waiting = false;
        return std::isfinite(Now) && std::isfinite(MaxAge) && MaxAge > 0 && Now >= Pending.SentAt && Now - Pending.SentAt <= MaxAge;
    }
    void Abort() { Waiting = false; }
    bool HasPending() const { return Waiting; }
    bool Expired(double Now, double MaxAge = 1.0) const
    {
        return Waiting && std::isfinite(Now) && Now - Pending.SentAt > MaxAge;
    }
    std::uint64_t Command() const { return Generation; }
private:
    std::uint64_t Generation = 1;
    std::uint64_t Sequence = 0;
    Ticket Pending;
    bool Waiting = false;
};
}
