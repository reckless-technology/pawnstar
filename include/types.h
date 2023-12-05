#pragma once
#include "options.h"
#include <stdbool.h>
#include <stdint.h>

typedef uint64_t bitboard; /**<  64 bit unsigned bitset mapping to squares on the chess board. */

/**
 * @brief Chess pieces.
*/
enum Piece
{
    NO_PIECE,
    PAWN,
    KNIGHT,
    BISHOP,
    ROOK,
    QUEEN,
    KING,
};

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
 * @brief Bitset of castling rights flags.
*/
enum CastleFlags
{
    MAY_WHITE_CASTLE_KINGSIDE   = 0x01, /**< white has the right to castle king side    */
    MAY_WHITE_CASTLE_QUEENSIDE  = 0x02, /**< white has the right to castle queen side   */
    MAY_BLACK_CASTLE_KINGSIDE   = 0x04, /**< black has the right to castle king side    */
    MAY_BLACK_CASTLE_QUEENSIDE  = 0x08, /**< black has the right to castle queen side   */
};

/**
 * @brief Bitset of position state flags.
*/
enum StateFlags
{                                                                                               
    IS_BLACK_TO_MOVE    = 0x01, /**< it is black's turn to move                                 */
    IS_CHECK            = 0x02, /**< is the side to move in check                               */
    IS_MOVED_INTO_CHECK = 0x04, /**< is the side not to move in check (illegal position)        */
    IS_CHECKMATE        = 0x08, /**< position represents checkmate                              */
    IS_STALEMATE        = 0x10, /**< position represents stalemate                              */
    IS_DRAW_REPETITION  = 0x20, /**< position represents draw by repetition 3 times             */
    IS_DRAW_MATERIAL    = 0x40, /**< position represents draw by insufficient material to mate  */
    IS_DRAW_50_MOVES    = 0x80, /**< position represents draw by the 50 move rule               */
    IS_GAME_DRAWN       = (IS_STALEMATE | IS_DRAW_REPETITION | IS_DRAW_MATERIAL | IS_DRAW_50_MOVES),
    IS_GAME_OVER        = (IS_GAME_DRAWN | IS_CHECKMATE),
};

/**
 * @brief Move generation and search phases.
*/
enum MovePhase
{
    PHASE_PRE_MOVES,        /**< moves from the principal variation or transposition table              */
    PHASE_CAPTURES,         /**< capture and promotion moves with a non negative static exchange eval   */
    PHASE_NON_CAPTURES,     /**< non captures moves with a non negative static exchange eval            */
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
 * @brief A move and its associated score - used when sorting moves.
*/
typedef struct
{
    int move;
    int score;
} ScoredMove;

/**
 * @brief Bitboard bit sets for a specific square.
*/
typedef struct
{
    union
    {
        bitboard pawn_attacks[2];   /**< indexed by color */
        struct
        {
            bitboard pawn_attacks_white;    /**< squares attacked by a white pawn on this square    */
            bitboard pawn_attacks_black;    /**< squares attacked by a black pawn on this square    */
        };
    };
    bitboard knight_attacks;    /**< squares attacked by a knight on this square    */
    bitboard bishop_attacks;    /**< squares attacked by a bishop on an empty board */
    bitboard rook_attacks;      /**< squares attacked by a rook on an empty board   */
    bitboard queen_attacks;     /**< squares attacked by a queen on an empty board  */
    bitboard king_attacks;      /**< squares attacked by a king on this square      */
} Sets;

/**
 * @brief Bitsets used to efficiently determine pawn structure.
*/
typedef struct
{
    bitboard passed_pawn_mask;      /**< Squares which if not containing an enemy pawn, make the pawn passed.       */
    bitboard isolated_pawn_mask;    /**< Squares which if not containing a friendly pawn, make the pawn isolated.   */
    bitboard supported_pawn_mask;   /**< Squares which if containing a friendly pawn, make the pawn supported.      */
    bitboard doubled_pawn_mask;     /**< Squares which if containing a friendly pawn, make the pawn doubled.        */
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
        bitboard pieces[7];             /**< used to index pieces by piece type */
        struct
        {
            bitboard occupied_squares;  /**< all squares with a piece on them   */
            bitboard pawns;             /**< squares with a pawn on them        */
            bitboard knights;           /**< squares with a knight on them      */  
            bitboard bishops;           /**< squares with a bishop on them      */
            bitboard rooks;             /**< squares with a rook on them        */
            bitboard queens;            /**< squares with a queen on them       */
            bitboard kings;             /**< squares with a king on them        */
        };
    };
    union
    {
        bitboard pieces_of_color[2];    /**< used to index pieces by color */
        struct
        {
            bitboard white_pieces;      /**< squares with a white piece on them */
            bitboard black_pieces;      /**< squares with a black piece on them */
        };
    };    
    uint64_t        hash;                   /**< Zobrist hash of this position, maintained incrementally    */
    const Position* previous;               /**< position immediately prior to this in the line of play     */
    int             move;                   /**< the move which led to this position                        */
    uint8_t         castle_flags;           /**< castling rights                                            */
    uint8_t         state_flags;            /**< game state-machine flags                                   */
    uint8_t         en_passant_index;       /**< en passant capture availability square (0 if none)         */
    uint8_t         reversible_move_count;  /**< number of consecutive reversible half-moves (plies)        */
    uint8_t         full_move_count;        /**< number of full moves (zero indexed)                        */
};

/**
 * @brief Clock for a standard time control game.
*/
typedef struct
{
    int     milliseconds_remaining;  /**< how many milliseconds does the computer have left on its clock             */
    int     milliseconds_per_period; /**< how many milliseconds in each period for a standard chess clock            */
    int     moves_per_period;        /**< how many moves in each period for a standard chess clock                   */
} StandardTimeControl;

/**
 * @brief Clock for an incremental time control game.
*/
typedef struct
{
    int     milliseconds_remaining;  /**< how many milliseconds does the computer have left on its clock             */
    int     base_milliseconds;       /**< how many milliseconds for the game for an incremental chess clock          */
    int     increment_milliseconds;  /**< how many additional milliseconds per move for an incremental chess clock   */
} IncrementalTimeControl;

/**
 * @brief Clock for a fixed depth of search game.
*/
typedef struct
{
    int     depth;                  /**< search depth to search to when using fixed depth                           */
} FixedDepthTimeControl;

/**
 * @brief Clock for a fixed time per move game.
*/
typedef struct
{
    int     milliseconds;           /**< how many milliseconds to spend on each move when using fixed time          */
} FixedTimeTimeControl;

/**
 * @brief Clock and time controls.
*/
typedef struct
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
typedef struct
{
    uint64_t    hash;       /**< Zobrist hash of this position                          */
    int         move;       /**< Best move from this position, if any                   */
    int16_t     score;      /**< The score computed from this position                  */
    int8_t      depth;      /**< The depth to which this position was searched          */
    uint8_t     node_type;  /**< The alpha-beta tree search node type (cut, all, pv)    */
} Transposition;

/**
 * @brief A variation, or line of play.
 * Typically used to record the principal variation during search.
*/
typedef struct
{
    int moves[MAX_PLY + 1]; /**< the moves comprising the line  */
} Variation;

/**
 * @brief A chess game.
*/
typedef struct
{
    Position*       position;               /**< stack pointer - current position               */
    Position        stack[MAX_GAME_LENGTH]; /**< position stack                                 */
    TimeControl     time_control;           /**< Clock controls for the current game            */
    int             node_count;             /**< Number of nodes (positions) during search      */
    int             engine_color;           /**< The color which pawnstar is playing            */
    bool            do_show_thinking;       /**< Whether to show scores and PV during search    */
} Game;