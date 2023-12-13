#pragma once
#include "options.h"
#include <stdbool.h>
#include <stdint.h>

#include "bitboard.h"
#include "move.h"
#include "position.h"

/**
 * @brief Time control clock types.
*/
enum ClockType
{
    CLOCK_STANDARD,     /**< N moves to be made in M minutes                */
    CLOCK_INCREMENTAL,  /**< M minutes for the game plus N seconds per move */
    CLOCK_FIXED_DEPTH,  /**< search to depth D on every move                */
    CLOCK_FIXED_TIME,   /**< search for N seconds on every move             */
};

/**
 * @brief Chess board square indices.
*/
enum Squares
{
    A1, B1, C1, D1, E1, F1, G1, H1,
    A2, B2, C2, D2, E2, F2, G2, H2,
    A3, B3, C3, D3, E3, F3, G3, H3,
    A4, B4, C4, D4, E4, F4, G4, H4,
    A5, B5, C5, D5, E5, F5, G5, H5,
    A6, B6, C6, D6, E6, F6, G6, H6,
    A7, B7, C7, D7, E7, F7, G7, H7,
    A8, B8, C8, D8, E8, F8, G8, H8,
};

/**
 * @brief Alpha beta search tree node types.
*/
enum NodeType
{
    NODE_CUT,   /**< beta cutoff occurred       */
    NODE_ALL,   /**< no move exceeded alpha     */
    NODE_PV,    /**< principal variation node   */
};





/**
 * @brief Clock for a standard time control game.
*/
struct StandardTimeControl
{
    int     milliseconds_remaining;  /**< how many milliseconds does the computer have left on its clock             */
    int     milliseconds_per_period; /**< how many milliseconds in each period for a standard chess clock            */
    int     moves_per_period;        /**< how many moves in each period for a standard chess clock                   */
};

/**
 * @brief Clock for an incremental time control game.
*/
struct IncrementalTimeControl
{
    int     milliseconds_remaining;  /**< how many milliseconds does the computer have left on its clock             */
    int     base_milliseconds;       /**< how many milliseconds for the game for an incremental chess clock          */
    int     increment_milliseconds;  /**< how many additional milliseconds per move for an incremental chess clock   */
};

/**
 * @brief Clock for a fixed depth of search game.
*/
struct FixedDepthTimeControl
{
    int     depth;                  /**< search depth to search to when using fixed depth                           */
};

/**
 * @brief Clock for a fixed time per move game.
*/
struct FixedTimeTimeControl
{
    int     milliseconds;           /**< how many milliseconds to spend on each move when using fixed time          */
};

/**
 * @brief Clock and time controls.
*/
struct TimeControl
{
    int             hard_stop_search_ms;    /**< Wall clock time to hard stop searching and move    */
    enum ClockType  clock_type;             /**< standard chess clock, incremental clock, fixed time or fixed depth         */
    union
    {
        StandardTimeControl     standard;
        IncrementalTimeControl  incremental;
        FixedDepthTimeControl   fixed_depth;
        FixedTimeTimeControl    fixed_time;
    };
};

/**
 * @brief A transposition table entry.
 * A transposition is the result of a previous search containing pertinent
 * information about what was found when this position was searched earlier.
*/
struct Transposition
{
    uint64_t    hash;       /**< Zobrist hash of this position                          */
    Move        move;       /**< Best move from this position, if any                   */
    int         score;      /**< The score computed from this position                  */
    int16_t     depth;      /**< The depth to which this position was searched          */
    uint16_t    node_type;  /**< The alpha-beta tree search node type (cut, all, pv)    */
};

/**
 * @brief A variation, or line of play.
 * Typically used to record the principal variation during search.
*/
struct Variation
{
    Move moves[MAX_PLY + 1]; /**< the moves comprising the line  */
};

/**
 * @brief A chess game.
*/
struct Game
{
    Position*   position;               /**< stack pointer - current position               */
    Position    stack[MAX_GAME_LENGTH]; /**< position stack                                 */
    TimeControl time_control;           /**< Clock controls for the current game            */
    int         node_count;             /**< Number of nodes (positions) during search      */
    int         engine_color;           /**< The color which pawnstar is playing            */
    bool        do_show_thinking;       /**< Whether to show scores and PV during search    */
};



/**
 * @brief Holds information about pinned pieces.
 */
struct Pins
{
    Bitboard pinned_pieces;         /**< Set of squares which are pinned. */
    Bitboard allowed_squares[64];   /**< Contains the set of allowed squares a pinned piece may safely move to. */
};

/**
 * @brief Pawn structure for one color.
 * A passed pawn has no enemy pawns in front of it either on its file or on adjacent files.
 * 
 * An isolated pawn has no friendly pawns on either adjacent file.
 * 
 * A backward pawn has no pawns to support it and its forward square is 
 * attacked by an enemy pawn.
 * 
 * A doubled pawn has a friendly pawn in front of it on the same file.
 */
struct PawnStructure
{
    Bitboard passed_pawns;
    Bitboard isolated_pawns;
    Bitboard backward_pawns;
    Bitboard doubled_pawns;
};

