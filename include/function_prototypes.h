#pragma once
#include <stdio.h>
#include <string>

#include "bitboard.h"
#include "move.h"
#include "generated_data.h"

struct Position;

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

int         Search          (const Position& position, int depth, int ply, int alpha, int beta, volatile bool& cancel, Variation& parent_pv);
int         SearchQuiescent (const Position& position, int depth, int ply, int alpha, int beta, volatile bool& cancel);
int         SearchSingleMove(const Position& position, int depth, int ply, int alpha, int beta, volatile bool& cancel, Move move, Variation& pv, int move_index);
Move        SearchRootNode  (const Position& position);

int         GetMilliseconds(void);

int         MoveSequenceToSanString(const Position& position, const Move* moves, char* move_string);
int         MoveToString(const Position& position, Move move, char* move_string);

void        DisplayAvailableBookMoves(const Position& position);
void        FreeOpeningBook(void);
Move        GetBookMove(uint64_t hash);
bool        InitializeOpeningBookFromFile(const char* filename);
bool        InitializeDefaultOpeningBook();

int         EvaluateStaticExchange(const Position& src_position, Move move);
void        InitializeGoodMoveCounts(void);
void        RecordGoodMove(int ply, Move move);
void        ScoreAndSortMoves(const Position& position, Move moves[], int ply, int depth);
void        SortMoves(int num_elements, Move values[]);
int         EvaluatePosition(const Position& position, int alpha, int beta);
bool        RunMergeSortTests(void);
void        RunPerftTests(void);
void        RunPositionTests(int depth);
void        RunStaticExchangeTests(void);
int         NextRandom(void);
void        ProcessInput(char* line);