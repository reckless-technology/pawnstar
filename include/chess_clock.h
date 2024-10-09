#pragma once
#include <cstdint>

/// @brief Time control chess clock types.
enum ClockType
{
    CHESS_CLOCK_STANDARD,    ///< N moves to be made in M minutes
    CHESS_CLOCK_FIXED_DEPTH, ///< Search to depth D on every move
    CHESS_CLOCK_FIXED_TIME,  ///< Search for N milliseconds on every move
    CHESS_CLOCK_INFINITE,    ///< Keep searching until told to stop
};

/// @brief Clock and time controls.
struct TimeControl
{

    ClockType clock_type;          ///< standard chess clock, incremental clock, fixed time or fixed depth
    int       hard_stop_ms;        ///< Wall clock time to hard stop searching and move
    int       ms_remaining;        ///< Number of ms remaining in this clock period.
    int       num_moves_remaining; ///< Number of moves remaining in this clock period.
    int       depth;               ///< Search depth (when CLOCK_FIXED_DEPTH is used).
};

/// @brief Get elapsed time.
/// @return Milliseconds since first invocation.
int64_t ElapsedMilliseconds();

int64_t ElapsedMicroseconds();