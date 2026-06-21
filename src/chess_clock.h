#pragma once
/// @file chess_clock.h Chess clock.
#include <chrono>
#include <cstdint>

/// @brief Clock and time controls.
class ChessClock
{
  public:
    /// @brief Time control chess clock types.
    enum ClockType
    {
        kStandard,   ///< N moves to be made in a specified time.
        kFixedDepth, ///< Search to depth D on every move.
        kFixedTime,  ///< Search for N milliseconds on every move.
        kInfinite,   ///< Keep searching until told to stop.
    };

    /// @brief Default constructor; standard clock with five minutes remaining, started now.
    ChessClock()
        : clock_type{kStandard}, hard_stop_ms{0}, ms_remaining{5 * 60 * 1000}, num_moves_remaining{0}, depth{10},
          search_start_ms{0}, ms_allocated{0}, ms_timeout{0}, start_time_{std::chrono::system_clock::now()}
    {
    }

    ClockType clock_type;          ///< The current clock type.
    int64_t   hard_stop_ms;        ///< Wall clock time to hard stop searching and move
    int       ms_remaining;        ///< Number of ms remaining in this clock period.
    int       num_moves_remaining; ///< Number of moves remaining in this clock period.
    int       depth;               ///< Search depth (when CLOCK_FIXED_DEPTH is used).
    // The following three are set per search by SearchRootNode and read live by the running search, so that
    // `ponderhit` can retarget a ponder search to a real budget mid-flight (budget-from-ponderhit). Plain
    // int64/int (aligned, naturally atomic on the x86-64 target); written by the UCI thread, read by workers.
    int64_t   search_start_ms;     ///< Elapsed-ms timestamp the soft-time budget is measured from (reset on ponderhit).
    int       ms_allocated;        ///< Soft time budget for this move (ms).
    int       ms_timeout;          ///< Hard-stop budget for this move (ms); used to set hard_stop_ms on ponderhit.

    /// @brief Elapsed time since the clock was constructed.
    /// @return Microseconds elapsed.
    int64_t ElapsedMicroseconds()
    {
        return duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now() - start_time_).count();
    }

    /// @brief Elapsed time since the clock was constructed.
    /// @return Milliseconds elapsed.
    int64_t ElapsedMilliseconds()
    {
        return ElapsedMicroseconds() / 1000;
    }

  private:
    std::chrono::system_clock::time_point start_time_; ///< When the clock was started.
};