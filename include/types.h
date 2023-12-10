#pragma once
#include "options.h"
#include <stdbool.h>
#include <stdint.h>

#include "bitboard.h"
#include "move.h"

/**
 * @brief Chess piece colors.
*/
enum Color
{
    WHITE,
    BLACK,
    NEITHER_COLOR,
};

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
 * @brief Bitset of position state flags.
*/
enum StateFlags
{
    MAY_WHITE_CASTLE_KINGSIDE   = 1 <<  0,  /**< white has the right to castle king side                    */
    MAY_WHITE_CASTLE_QUEENSIDE  = 1 <<  1,  /**< white has the right to castle queen side                   */
    MAY_BLACK_CASTLE_KINGSIDE   = 1 <<  2,  /**< black has the right to castle king side                    */
    MAY_BLACK_CASTLE_QUEENSIDE  = 1 <<  3,  /**< black has the right to castle queen side                   */
    IS_BLACK_TO_MOVE            = 1 <<  4,  /**< it is black's turn to move                                 */
    IS_CHECK                    = 1 <<  5,  /**< is the side to move in check                               */
    IS_MOVED_INTO_CHECK         = 1 <<  6,  /**< is the side not to move in check (illegal position)        */
    IS_CHECKMATE                = 1 <<  7,  /**< position represents checkmate                              */
    IS_STALEMATE                = 1 <<  8,  /**< position represents stalemate                              */
    IS_DRAW_REPETITION          = 1 <<  9,  /**< position represents draw by repetition 3 times             */
    IS_DRAW_MATERIAL            = 1 << 10,  /**< position represents draw by insufficient material to mate  */
    IS_DRAW_50_MOVES            = 1 << 11,  /**< position represents draw by the 50 move rule               */
    IS_NULL_MOVE                = 1 << 12,  /**< position was the result of a null move                     */
    IS_GAME_DRAWN               = (IS_STALEMATE | IS_DRAW_REPETITION | IS_DRAW_MATERIAL | IS_DRAW_50_MOVES),
    IS_GAME_OVER                = (IS_GAME_DRAWN | IS_CHECKMATE),
    CASTLING_RIGHTS_MASK        = (MAY_WHITE_CASTLE_KINGSIDE | MAY_WHITE_CASTLE_QUEENSIDE | 
                                   MAY_BLACK_CASTLE_KINGSIDE | MAY_BLACK_CASTLE_QUEENSIDE),
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
 * @brief Bitboard bit sets for a specific square.
*/
typedef struct Sets
{
    Bitboard north;
    Bitboard northeast;
    Bitboard east;
    Bitboard southeast;
    Bitboard south;
    Bitboard southwest;
    Bitboard west;
    Bitboard northwest;
    union
    {
        Bitboard pawn_attacks[2];   /**< indexed by color */
        struct
        {
            Bitboard pawn_attacks_white;    /**< squares attacked by a white pawn on this square    */
            Bitboard pawn_attacks_black;    /**< squares attacked by a black pawn on this square    */
        };
    };
    Bitboard knight_attacks;    /**< squares attacked by a knight on this square    */
    Bitboard bishop_attacks;    /**< squares attacked by a bishop on an empty board */
    Bitboard rook_attacks;      /**< squares attacked by a rook on an empty board   */
    Bitboard queen_attacks;     /**< squares attacked by a queen on an empty board  */
    Bitboard king_attacks;      /**< squares attacked by a king on this square      */
    Bitboard king_attacks2;     /**< squares within 2 king moves from this square   */
} Sets;

/**
 * @brief Bitsets used to efficiently determine pawn structure.
*/
typedef struct PawnSets
{
    Bitboard passed_pawn_mask;      /**< Squares which if not containing an enemy pawn, make the pawn passed.       */
    Bitboard isolated_pawn_mask;    /**< Squares which if not containing a friendly pawn, make the pawn isolated.   */
    Bitboard supported_pawn_mask;   /**< Squares which if containing a friendly pawn, make the pawn supported.      */
    Bitboard doubled_pawn_mask;     /**< Squares which if containing a friendly pawn, make the pawn doubled.        */
} PawnSets;


/**
 * @brief A chess position.
 * The position comprises the pieces on the board, whose turn it is to
 * move, castling rights for each side, whether en passant capture is possible,
 * and the number of consecutive reversible half-moves.
*/
typedef struct Position Position;
struct Position
{    
    union
    {
        Bitboard piece[6];              /**< used to index pieces by [piece - 1] */
        struct
        {
            Bitboard pawns;             /**< squares with a pawn on them        */
            Bitboard knights;           /**< squares with a knight on them      */  
            Bitboard bishops;           /**< squares with a bishop on them      */
            Bitboard rooks;             /**< squares with a rook on them        */
            Bitboard queens;            /**< squares with a queen on them       */
            Bitboard kings;             /**< squares with a king on them        */
        };
    };
    union
    {
        Bitboard pieces_of_color[2];    /**< used to index pieces by color */
        struct
        {
            Bitboard white_pieces;      /**< squares with a white piece on them */
            Bitboard black_pieces;      /**< squares with a black piece on them */
        };
    };    
    uint64_t        hash;                   /**< Zobrist hash of this position, maintained incrementally    */
    const Position* previous;               /**< position immediately prior to this in the line of play     */
    uint16_t        flags;                  /**< game state-machine flags                                   */
    uint8_t         king_location[2];       /**< square index of white and black kings                      */
    uint8_t         en_passant_index;       /**< en passant capture availability square (0 if none)         */
    uint8_t         reversible_move_count;  /**< number of consecutive reversible half-moves (plies)        */
    uint8_t         full_move_count;        /**< number of full moves (zero indexed)                        */
};

/**
 * @brief Clock for a standard time control game.
*/
typedef struct StandardTimeControl
{
    int     milliseconds_remaining;  /**< how many milliseconds does the computer have left on its clock             */
    int     milliseconds_per_period; /**< how many milliseconds in each period for a standard chess clock            */
    int     moves_per_period;        /**< how many moves in each period for a standard chess clock                   */
} StandardTimeControl;

/**
 * @brief Clock for an incremental time control game.
*/
typedef struct IncrementalTimeControl
{
    int     milliseconds_remaining;  /**< how many milliseconds does the computer have left on its clock             */
    int     base_milliseconds;       /**< how many milliseconds for the game for an incremental chess clock          */
    int     increment_milliseconds;  /**< how many additional milliseconds per move for an incremental chess clock   */
} IncrementalTimeControl;

/**
 * @brief Clock for a fixed depth of search game.
*/
typedef struct FixedDepthTimeControl
{
    int     depth;                  /**< search depth to search to when using fixed depth                           */
} FixedDepthTimeControl;

/**
 * @brief Clock for a fixed time per move game.
*/
typedef struct FixedTimeTimeControl
{
    int     milliseconds;           /**< how many milliseconds to spend on each move when using fixed time          */
} FixedTimeTimeControl;

/**
 * @brief Clock and time controls.
*/
typedef struct TimeControl
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
} TimeControl;



/**
 * @brief A transposition table entry.
 * A transposition is the result of a previous search containing pertinent
 * information about what was found when this position was searched earlier.
*/
typedef struct Transposition
{
    uint64_t    hash;       /**< Zobrist hash of this position                          */
    Move        move;       /**< Best move from this position, if any                   */
    int         score;      /**< The score computed from this position                  */
    int16_t     depth;      /**< The depth to which this position was searched          */
    uint16_t    node_type;  /**< The alpha-beta tree search node type (cut, all, pv)    */
} Transposition;

/**
 * @brief A variation, or line of play.
 * Typically used to record the principal variation during search.
*/
typedef struct Variation
{
    Move moves[MAX_PLY + 1]; /**< the moves comprising the line  */
} Variation;

/**
 * @brief A chess game.
*/
typedef struct Game
{
    Position*   position;               /**< stack pointer - current position               */
    Position    stack[MAX_GAME_LENGTH]; /**< position stack                                 */
    TimeControl time_control;           /**< Clock controls for the current game            */
    int         node_count;             /**< Number of nodes (positions) during search      */
    int         engine_color;           /**< The color which pawnstar is playing            */
    bool        do_show_thinking;       /**< Whether to show scores and PV during search    */
} Game;

/**
 * @brief An entry in the magic bitboard move generator array.
 * Each entry contains information to generate sliding moves
 * for either a bishop or a rook, for one square.
 * Uses an extra indirection via indices, since indices are 1 byte
 * each and attacks are 8 bytes each. This saves a lot of space
 * in the (already very large) rook magic bitboard tables, since for
 * exmaple for rooks there are 12 bits in the occupancy mask (4096 indices)
 * but typically only around 100 actual move sets.
 * 
 * To get sliding move targets without branches or loops we use:
 * 
 * m->attacks[m->indices[((occupied_squares & m->occupancy_mask) * m->magic) >> m->shift]]
 * 
 * The magic bitboard sets are generated at compile time by 
 * compiling and then running "generate_constants.cpp"
 */
typedef struct MagicMoveEntry
{
    uint64_t        magic;          /**< Magic multiplier.                              */
    Bitboard        occupancy_mask; /**< Occupancy mask (excludes final target square). */
    int             shift;          /**< Number of bits to right shift to get indices.  */
    const uint64_t* attacks;        /**< Discrete attack vectors (move sets).           */
    const uint8_t*  indices;        /**< Indices into the discrete attack vector array. */
} MagicMoveEntry;

/**
 * @brief Holds information about pinned pieces.
 */
typedef struct Pins
{
    Bitboard pinned_pieces;         /**< Set of squares which are pinned. */
    Bitboard allowed_squares[64];   /**< Contains the set of allowed squares a pinned piece may safely move to. */
} Pins;