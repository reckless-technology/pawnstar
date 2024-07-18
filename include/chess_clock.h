#pragma once

/**
 * @brief Time control clock types.
 */
enum ClockType
{
    CLOCK_STANDARD,    /**< N moves to be made in M minutes                */
    CLOCK_INCREMENTAL, /**< M minutes for the game plus N seconds per move */
    CLOCK_FIXED_DEPTH, /**< search to depth D on every move                */
    CLOCK_FIXED_TIME,  /**< search for N seconds on every move             */
};

/**
 * @brief Clock for a standard time control game.
 */
struct StandardTimeControl
{
    int milliseconds_remaining;  /**< how many milliseconds does the computer have left on its clock             */
    int milliseconds_per_period; /**< how many milliseconds in each period for a standard chess clock            */
    int moves_per_period;        /**< how many moves in each period for a standard chess clock                   */
};

/**
 * @brief Clock for an incremental time control game.
 */
struct IncrementalTimeControl
{
    int milliseconds_remaining; /**< how many milliseconds does the computer have left on its clock             */
    int base_milliseconds;      /**< how many milliseconds for the game for an incremental chess clock          */
    int increment_milliseconds; /**< how many additional milliseconds per move for an incremental chess clock   */
};

/**
 * @brief Clock for a fixed depth of search game.
 */
struct FixedDepthTimeControl
{
    int depth; /**< search depth to search to when using fixed depth                           */
};

/**
 * @brief Clock for a fixed time per move game.
 */
struct FixedTimeTimeControl
{
    int milliseconds; /**< how many milliseconds to spend on each move when using fixed time          */
};

/**
 * @brief Clock and time controls.
 */
struct TimeControl
{
    int       hard_stop_search_ms; /**< Wall clock time to hard stop searching and move    */
    ClockType clock_type;          /**< standard chess clock, incremental clock, fixed time or fixed depth         */
    union {
        StandardTimeControl    standard;
        IncrementalTimeControl incremental;
        FixedDepthTimeControl  fixed_depth;
        FixedTimeTimeControl   fixed_time;
    };
};

/**
 * @brief Get the elapsed time.
 * @return Number of milliseconds elapsed since the program started.
 */
int ElapsedMilliseconds(void);