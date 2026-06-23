#pragma once
/// @file chess_clock.h Chess clock and per-search time management.
///
/// Times are `std::chrono` values on a monotonic (steady) clock: budgets and the remaining clock are
/// `Duration`s, and the hard stop is an absolute `TimePoint` deadline. Soft-time management compares the time
/// elapsed since the search started against the allocated budget; the hard deadline is the wall-clock instant
/// at which the search must stop no matter what.
#include <atomic>
#include <chrono>
#include <cstdint>

/// @brief Clock and time controls for the current game / search.
class ChessClock
{
  public:
    using Clock     = std::chrono::steady_clock; ///< Monotonic clock — correct for elapsed time and deadlines.
    using TimePoint = Clock::time_point;         ///< An absolute instant on the steady clock.
    using Duration  = std::chrono::milliseconds; ///< Budgets / remaining time (UCI granularity is the ms).

    /// @brief Time control types.
    enum ClockType
    {
        kStandard,   ///< N moves in a given time (or sudden death): time is managed per move.
        kFixedDepth, ///< Search to a fixed depth every move.
        kFixedTime,  ///< Search for a fixed amount of time every move.
        kInfinite,   ///< Search until told to stop.
    };

    ChessClock() = default;
    ClockType clock_type_{kStandard};                   ///< The current time-control type.
    Duration  remaining_time_{std::chrono::minutes{5}}; ///< Time left on our clock this period (UCI wtime/btime).
    Duration  increment_{
        Duration::zero()}; ///< Per-move increment, side to move (UCI winc/binc). Parsed but not currently used by time
                            ///< planning — see SearchRootNode (folding it into the budget SPRT'd neutral/negative).
    int      moves_to_go_{0}; ///< Moves until the next time control (0 = unknown / sudden death).
    int      depth_{10};      ///< Target depth when clock_type == kFixedDepth.
    uint64_t max_nodes_{0};   ///< Node limit for `go nodes` (0 = no limit); checked per-thread in Search.
    void     Reset();
    void     StartSearch(Duration allocated, Duration maximum);
    void     StartPonderSearch(Duration allocated, Duration maximum);
    void     OnPonderhit();
    Duration ElapsedSinceSearchStart() const;
    bool     HasReachedHardDeadline() const;

  private:
    /// @brief Sentinel hard deadline meaning "no hard stop" — `HasReachedHardDeadline` never returns true.
    static constexpr TimePoint kNoDeadline = TimePoint::max();
    void                       ArmHardDeadline(Duration maximum);

    // search_start_time_ and hard_deadline_ are mutated mid-search by `ponderhit` (UCI thread) and read every
    // node by the search worker threads, so they are atomic. The budgets are set before the workers start and
    // not changed mid-search, so they are plain.
    std::atomic<TimePoint> search_start_time_{Clock::now()};
    std::atomic<TimePoint> hard_deadline_{kNoDeadline};

  public:
    Duration allocated_time_{Duration::zero()}; ///< Soft time budget for this move (drives between-iteration stops).

  private:
    Duration max_search_time_{Duration::zero()};
};

/// @brief Reset to default time control (called when starting a new game). Provided instead of assigning a
/// fresh ChessClock because the atomic members below make the class non-assignable.
inline void ChessClock::Reset()
{
    clock_type_      = kStandard;
    remaining_time_  = std::chrono::minutes{5};
    increment_       = Duration::zero();
    moves_to_go_     = 0;
    depth_           = 10;
    max_nodes_       = 0;
    allocated_time_  = Duration::zero();
    max_search_time_ = Duration::zero();
    search_start_time_.store(Clock::now(), std::memory_order_relaxed);
    hard_deadline_.store(kNoDeadline, std::memory_order_relaxed);
}

/// @brief Begin timing a search: record the start instant and the soft/hard budgets, and arm the hard
/// deadline. A non-positive @p maximum means no hard deadline (fixed-depth / infinite).
/// @param allocated Soft time budget for the move. @param maximum Hard time budget (deadline = now + this).
inline void ChessClock::StartSearch(Duration allocated, Duration maximum)
{
    allocated_time_  = allocated;
    max_search_time_ = maximum;
    search_start_time_.store(Clock::now(), std::memory_order_relaxed);
    ArmHardDeadline(maximum);
}

/// @brief Begin a ponder search: timed like StartSearch but with NO hard deadline — the search runs until
/// `ponderhit` or `stop`. The budgets are remembered so OnPonderhit can start the real clock.
/// @param allocated Soft budget to use once pondering converts to a real search.
/// @param maximum Hard budget to use once pondering converts (the deadline OnPonderhit will arm).
inline void ChessClock::StartPonderSearch(Duration allocated, Duration maximum)
{
    allocated_time_  = allocated;
    max_search_time_ = maximum;
    search_start_time_.store(Clock::now(), std::memory_order_relaxed);
    hard_deadline_.store(kNoDeadline, std::memory_order_relaxed);
}

/// @brief The opponent played the pondered move: start the real clock now (budget-from-ponderhit), keeping
/// the work already done. Resets the soft-time origin and arms the hard deadline from this instant.
inline void ChessClock::OnPonderhit()
{
    search_start_time_.store(Clock::now(), std::memory_order_relaxed);
    ArmHardDeadline(max_search_time_);
}

/// @brief Time elapsed since the most recent search start (or ponderhit).
/// @return Elapsed duration (milliseconds granularity).
inline ChessClock::Duration ChessClock::ElapsedSinceSearchStart() const
{
    return std::chrono::duration_cast<Duration>(Clock::now() - search_start_time_.load(std::memory_order_relaxed));
}

/// @brief Whether the hard deadline has passed. Always false when there is no hard deadline.
/// @return true if the search must stop now.
inline bool ChessClock::HasReachedHardDeadline() const
{
    return Clock::now() >= hard_deadline_.load(std::memory_order_relaxed);
}

/// @brief Arm the hard deadline at now + @p maximum, or clear it (no hard stop) if @p maximum <= 0.
inline void ChessClock::ArmHardDeadline(Duration maximum)
{
    hard_deadline_.store(maximum > Duration::zero() ? Clock::now() + maximum : kNoDeadline, std::memory_order_relaxed);
}
