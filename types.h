#pragma once
#include "options.h"
#include <stdbool.h>
/******************************************************************************
Integral type definitions
*******************************************************************************/
typedef unsigned long long  bitboard;
typedef unsigned long long  uint64;
typedef unsigned char       uchar;
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
Move types
*******************************************************************************/
enum MoveType
{
    NON_CAPTURE,                // non capture by knight, bishop, rook, queen or king           
    SINGLE_PAWN_PUSH,           // pawn moves forward one square
    DOUBLE_PAWN_PUSH,           // pawn moves forward two squares
    CASTLING,                   // castling move
    EN_PASSANT_CAPTURE,         // pawn takes pawn en passant
    CAPTURE,                    // capture by a knight, bishop, rook, queen or king
    PAWN_PROMOTION_NON_CAPTURE, // pawn push to promotion
    PAWN_PROMOTION_CAPTURE,     // pawn captures and simultaneously promotes
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
    HAS_WHITE_CASTLED   = 0x10, // white has castled
    HAS_BLACK_CASTLED   = 0x20, // black has castled
    ALL_RIGHTS          = (MAY_WHITE_K | MAY_WHITE_Q | MAY_BLACK_K | MAY_BLACK_Q),
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
Compass directions - North is forward from white's perspective.
*******************************************************************************/
enum Direction
{
    DIR_NONE,
    DIR_NORTH,
    DIR_NORTHEAST,
    DIR_EAST,
    DIR_SOUTHEAST,
    DIR_SOUTH,
    DIR_SOUTHWEST,
    DIR_WEST,
    DIR_NORTHWEST,
};
/******************************************************************************
States of a search task
*******************************************************************************/
enum TaskState
{
    TASK_PENDING,   // task is waiting to be scheduled
    TASK_RUNNING,   // task is running
    TASK_COMPLETED, // task completed
    TASK_ABORTED,   // task was pre-emptively aborted (due to cutoff)
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
Structure to hold a move and its associated score - used when sorting moves
*******************************************************************************/
typedef struct
{
    int move;   // the move
    int score;  // its associated value
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
    const Position* previous;       // position immediately prior to this
    uint64  hash;                   // Zobrist hash of this position
    int     move;                   // the move which led to this position
    uchar   king_location[2];       // square index on which each king sits 
    uchar   castle_flags;           // castling rights and has-castled flags
    uchar   state_flags;            // game state-machine flags
    uchar   en_passant_index;       // en passant capture availability square (0 if none)
    uchar   reversible_move_count;  // number of consecutive reversible half-moves (plies)
    uchar   full_move_count;        // number of full moves (NB: zero indexed)
};
/******************************************************************************
Move generation context for resumable move generator
*******************************************************************************/
typedef struct
{   
    const Position* position;  
    bitboard        promotions_west;
    bitboard        promotions_east;
    bitboard        promotions_forward;
    bitboard        pawn_captures_en_passant;
    bitboard        pawn_single_pushes;
    bitboard        pawn_double_pushes;
    bitboard        friendly_pieces;
    bitboard        enemy_pieces;
    bitboard        targets;
    bitboard        sources;
    bitboard        attacks_to;
    bitboard        seventh_rank;
    int             color;
    int             resume_line_num;
    int             pawn_push_delta;
    int             pawn_west_delta;
    int             pawn_east_delta;
    int             captured_piece;
    int             moving_piece;
    int             promoted_piece;
    int             from;
    int             to;
    bool            do_all_moves;
} MoveGenerator;
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
A game i.e. a sequence of positions
*******************************************************************************/
typedef struct
{
    Position*   position;           // stack pointer - current position
    Position    stack[MAX_GAME_LENGTH];
} Game;
/******************************************************************************
A transposition (result of a previous search)
16 bytes in size, should ideally be cache aligned
*******************************************************************************/
typedef struct
{
    uint64  hash;
    short   depth;
    short   score;
    int     move        : 24;
    int     node_type   :  8;
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
    bitboard    blocked_pawns;   // pawns with an enemy pawn ahead on the same file but not on either adjacent file
    bitboard    pawn_holes;      // squares which could not be defended by a pawn
    bitboard    outposts;        // enemy pawn holes attacked by a friendly pawn
} PawnStructure;

/******************************************************************************
The context of a parallel search task - used when searching a move on a worker 
thread
*******************************************************************************/
typedef struct SearchTask SearchTask;
struct SearchTask
{
    SearchTask*     next;
    SearchTask*     prev;
    const Position* src_position; 
    volatile long   task_state;
    int             move;
    int             move_index; 
    int             depth; 
    int             ply; 
    int             alpha; 
    int             beta;          
    int             score; 
    volatile bool   cancel;
};
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