#pragma once
#include "options.h"
#include <stdbool.h>
/******************************************************************************
Integral type definitions
*******************************************************************************/
typedef unsigned long long  bitboard;
typedef unsigned long long  uint64;
typedef unsigned char       uint8;
typedef signed char         int8;
typedef int (*CompareFn)    (const void*, const void*); /* sort predicate */
/******************************************************************************
Piece types
*******************************************************************************/
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
/******************************************************************************
Colors
*******************************************************************************/
enum Color
{
    WHITE,
    BLACK,
    NEITHER_COLOR,
};
/******************************************************************************
Time control clock types
*******************************************************************************/
enum ClockType
{
    CLOCK_STANDARD,     // N moves to be made in M minutes  
    CLOCK_INCREMENTAL,  // M minutes for the game plus N seconds per move
    CLOCK_FIXED_DEPTH,  // search to depth D on every move
    CLOCK_FIXED_TIME,   // search for N seconds on every move
};
/******************************************************************************
Position castling flags (bitset)
*******************************************************************************/
enum CastleFlags
{
    MAY_WHITE_K         = 0x01, // white has the right to castle king side
    MAY_WHITE_Q         = 0x02, // white has the right to castle queen side
    MAY_BLACK_K         = 0x04, // black has the right to castle king side
    MAY_BLACK_Q         = 0x08, // black has the right to castle queen side
};
/******************************************************************************
Position state flags (bitset)
*******************************************************************************/
enum StateFlags
{
    IS_BLACK_TO_MOVE    = 0x01, // it is black's turn to move
    IS_CHECK            = 0x02, // is the side to move in check
    MOVED_INTO_CHECK    = 0x04, // is the side not to move in check
    IS_CHECKMATE        = 0x08, // position represents checkmate
    IS_STALEMATE        = 0x10, // position represents stalemate
    IS_DRAW_REPETITION  = 0x20, // position represents draw by repetition 3 times
    IS_DRAW_MATERIAL    = 0x40, // position represents draw by insufficient material to mate
    IS_DRAW_50_MOVES    = 0x80, // position represents draw by the 50 move rule
    IS_GAME_DRAWN       = (IS_STALEMATE | IS_DRAW_REPETITION | IS_DRAW_MATERIAL | IS_DRAW_50_MOVES),
    IS_GAME_OVER        = (IS_GAME_DRAWN | IS_CHECKMATE),
};
/******************************************************************************
Search option flags (bitset)
*******************************************************************************/
enum SearchFlags
{
    IS_NULL_MOVE_OK     = 0x01, // is null move pruning permitted in this subtree
    IS_LMR_OK           = 0x02, // is late move reduction permitted in this subtree 
    IS_PVS_OK           = 0x04, // is PVS null window search permitted in this subtree
    IS_FOLLOWING_PV     = 0x08, // are we following the PV from the root node
    IS_DEFERRED_MOVE    = 0x10, // is this a deferred move with a negative SEE
    IS_PV_EXTN_OK       = 0x20, // is full window depth extension permitted in this subtree
    SEARCH_FLAG_ROOT    = IS_NULL_MOVE_OK | IS_LMR_OK | IS_FOLLOWING_PV | IS_PV_EXTN_OK,
};
/******************************************************************************
Phases of move search
*******************************************************************************/
enum MovePhase
{
    PHASE_PRE_MOVES,        // moves from the PV or TT (before move gen)
    PHASE_REGULAR_MOVES,    // regular moves with a non negative SEE
    PHASE_DEFERRED_MOVES,   // moves with a negative SEE
};
/******************************************************************************
Individual square indices (little endian rank file mapping)
*******************************************************************************/
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
/******************************************************************************
Search node types (used in transposition table entries)
*******************************************************************************/
enum NodeType
{
    NODE_CUT,       // beta cutoff occurred
    NODE_ALL,       // no move exceeded alpha
    NODE_PV,        // principal variation node
};
/******************************************************************************
A move / score pair - used when sorting moves
*******************************************************************************/
typedef struct
{
    int move;       // the move
    int score;      // its associated value
} ScoredMove;
/******************************************************************************
A chess position
*******************************************************************************/
typedef struct Position Position;
struct Position
{    
    union
    {
        bitboard pieces[7];        
        struct
        {
            bitboard occupied_squares;
            bitboard pawns;
            bitboard knights;
            bitboard bishops;
            bitboard rooks;
            bitboard queens;
            bitboard kings;
        };
    };
    union
    {
        bitboard pieces_of_color[2]; 
        struct
        {
            bitboard white_pieces;
            bitboard black_pieces;
        };
    };    
    uint64          hash;                   // Zobrist hash of this position
    const Position* previous;               // position immediately prior to this
    int             move;                   // the move which led to this position
    uint8           king_location[2];       // kings square indices
    uint8           castle_flags;           // castling rights
    uint8           state_flags;            // game state-machine flags
    uint8           en_passant_index;       // en passant capture availability square (0 if none)
    uint8           reversible_move_count;  // number of consecutive reversible half-moves (plies)
    uint8           full_move_count;        // number of full moves (zero indexed)
};
/******************************************************************************
Values for magic bitboard attack generator for one square
*******************************************************************************/
typedef struct
{
    uint64          magic;          // the multiplier
    bitboard        occupancy_mask; // mask for the pertinent occupied squares excluding outer squares
    const uint8*    attack_indices; // indices into the attacks set
    const bitboard* attacks;        // the set of distinct attacks 
    int             shift;          // right shift amount after multiplication
    int             padding;        // make the entry a multiple of 8 bytes in size
} MagicMoveEntry;
/******************************************************************************
Clock and time control information
*******************************************************************************/
typedef struct
{
    int     clock_type;              // standard, incremental, fixed time or fixed depth
    int     milliseconds_remaining;  // how many milliseconds does the computer have left on its clock
    int     milliseconds_per_period; // how many milliseconds in each period for a standard chess clock
    int     moves_per_period;        // how many moves in each period for a standard chess clock
    int     base_milliseconds;       // how many milliseconds for the game for an incremental chess clock
    int     increment_milliseconds;  // how many additional milliseconds per move for an incremental chess clock
    int     fixed_depth;             // search depth to search to when using fixed depth
    int     fixed_milliseconds;      // how many milliseconds to spend on each move when using fixed time
} TimeControl;
/******************************************************************************
A game of chess represented as a sequence of positions
*******************************************************************************/
typedef struct
{
    Position*   position;           // stack pointer - current position
    Position    stack[MAX_GAME_LENGTH];
} Game;
/******************************************************************************
A transposition (encompasses brief results of a previous search)
*******************************************************************************/
typedef struct
{
    uint64      hash;
    int         move;
    short       score;
    int8        depth;
    uint8       node_type;    
} Transposition;
/******************************************************************************
Information about pinned pieces and their possible move targets
*******************************************************************************/
typedef struct
{
    bitboard pinned_pieces;          // location of all pinned pieces
    bitboard allowed_squares[64];    // allowed_squares[x] indicates squares to which the pinned piece on x may safely move
} Pins;
/******************************************************************************
Pawn-structure features (one for each color)
*******************************************************************************/
typedef struct
{
    bitboard    pawn_attacks;    // squares directly attack by pawns
    bitboard    defended_pawns;  // pawns defended by a friendly pawn
    bitboard    isolated_pawns;  // pawns with no friendly pawn on either adjacent file
    bitboard    doubled_pawns;   // pawns with a friendly pawn ahead on the same file
    bitboard    passed_pawns;    // pawns who cannot be stopped by an enemy pawn
} PawnStructure;
/******************************************************************************
A sequence of moves for a (possibly principal) variation, i.e. line of play
*******************************************************************************/
typedef struct
{
    int         num_moves;      // number of moves in this line
    int         moves[MAX_PLY]; // the moves
} Variation;
/******************************************************************************
XBoard input command handling support
*******************************************************************************/
typedef struct
{
    void            (*function)(char* buffer);
    const char*     name;
    const char*     description;
} CommandHandler;
/******************************************************************************
Global variables are contained in a single instance of this struct 
*******************************************************************************/
typedef struct
{
    Game        game[1];
    TimeControl time_control;
    int         node_count;
    int         engine_color;
    bool        do_show_thinking;
} Context;