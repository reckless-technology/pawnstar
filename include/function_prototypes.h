#pragma once
#include <stdio.h>
#include <string>

#include "bitboard.h"
#include "types.h"
#include "generated_data.h"

constexpr uint8_t   FileOf(int locn)    { return locn & 7;              }
constexpr uint8_t   RankOf(int locn)    { return locn >> 3;             }
constexpr char      FileChar(int locn)  { return 'a' + FileOf(locn);    }
constexpr char      RankChar(int locn)  { return '1' + RankOf(locn);    }
constexpr uint8_t   EnemyOf(int color)  { return !color;                }
constexpr int       min(int a, int b)   { return a < b ? a : b;         }
constexpr int       max(int a, int b)   { return a > b ? a : b;         }

constexpr Bitboard 
BishopAttacks(Bitboard occupied_squares, uint8_t location)
{
    const MagicMoveEntry& m = BISHOP_MAGICS[location];
    return m.attacks[m.indices[((occupied_squares & m.occupancy_mask) * m.magic) >> m.shift]];
}

constexpr Bitboard 
RookAttacks(Bitboard occupied_squares, uint8_t location)
{
    const MagicMoveEntry& m = ROOK_MAGICS[location];
    return m.attacks[m.indices[((occupied_squares & m.occupancy_mask) * m.magic) >> m.shift]];
}

constexpr Bitboard 
QueenAttacks(Bitboard occupied_squares, uint8_t location)
{
    return BishopAttacks(occupied_squares, location) | RookAttacks(occupied_squares, location);
}

constexpr void 
CopyVariation(Variation&       dst_variation, 
              const Variation& src_variation, 
              Move             best_move)
{
    Move* dst = dst_variation.moves;
    *dst++ = best_move;
    const Move* src = src_variation.moves;
    while (*src)
    {
        *dst++ = *src++;
    }
    *dst = 0;
}

constexpr uint8_t
PieceAt(const Position& position, 
        int             location)
{
    const Bitboard square = BITBOARD(location);
    return 
        (square & position.pawns_)   ? PAWN   :
        (square & position.knights_) ? KNIGHT :
        (square & position.bishops_) ? BISHOP :
        (square & position.rooks_)   ? ROOK   :
        (square & position.queens_)  ? QUEEN  :
        (square & position.kings_)   ? KING   : NO_PIECE;
}

int         Search          (const Position& position, int depth, int ply, int alpha, int beta, volatile bool& cancel, Variation& parent_pv);
int         SearchQuiescent (const Position& position, int depth, int ply, int alpha, int beta, volatile bool& cancel);
int         SearchSingleMove(const Position& position, int depth, int ply, int alpha, int beta, volatile bool& cancel, Move move, Variation& pv, int move_index);
Move        SearchRootNode  (const Position& position);

int         GetMilliseconds(void);
void        StartThinking(Game& game);
void        StopThinking(void);
void        StopWorker(void);

void        InitializeGame(Game& game);
Move        PlayMoveString(Game& game, char* move_string);
int         MoveSequenceToSanString(const Position& position, const Move* moves, char* move_string);
int         MoveToString(const Position& position, Move move, char* move_string);

void        DisplayAvailableBookMoves(const Position& position);
void        FreeOpeningBook(void);
Move        GetBookMove(uint64_t hash);
bool        InitializeOpeningBookFromFile(const char* filename);
bool        InitializeDefaultOpeningBook();

bool        FindTransposition(uint64_t hash, Transposition* transposition);
void        FreeTranspositionTable(void);
bool        InitializeTranspositionTable(int megabytes);
void        RecordTransposition(uint64_t hash, int depth, int score, Move move, int node_type);

void        DeterminePins(const Position& position, Pins& pins);
int         EvaluateStaticExchange(const Position& src_position, Move move);
void        InitializeGoodMoveCounts(void);
void        RecordGoodMove(int ply, Move move);
int         GenerateLegalMoves(const Position& position, Move moves[]);
void        GeneratePseudoLegalCaptures(const Position& position, Move moves[]);
void        GeneratePseudoLegalMoves(const Position& position, Move moves[]);
void        ScoreAndSortMoves(const Position& position, Move moves[], int ply, int depth);
void        SortMoves(int num_elements, Move values[]);
int         EvaluatePosition(const Position& position, int alpha, int beta);
void        DisplayResultIfGameOver(const Position& position);
void        PlayMove(Game& game, Move move);
bool        RunMergeSortTests(void);
void        RunPerftTests(void);
void        RunPositionTests(int depth);
void        RunStaticExchangeTests(void);
int         NextRandom(void);
void        ProcessInput(char* line);


#if DEBUGX
extern DebugEntry debug_dict[DEBUG_DICT_SIZE];
/**
* @brief Increment the count associated with the string literal in key
*/
constexpr void DebugXIncrement(const char* key)
{
    /*
    String literals are typically aligned on 4 byte boundaries, so throw away
    the bottom 2 bits of the address before finding the index into the table
    */
    DebugEntry* const entry = &debug_dict[((uint64_t)key >> 2) % DEBUG_DICT_SIZE];
    entry->key = key;
    ++entry->count;
}

/**
* @brief Conditionally increment the count associated with the string literal in key
*/
constexpr void DebugXIncrementIf(bool condition, const char* key)
{
    if (condition)
    {
        DebugXIncrement(key);
    }
}

void    DebugXClear(void);
void    DebugXWrite(FILE* file);
#endif
