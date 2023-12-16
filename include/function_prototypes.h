#pragma once

#include <vector>
#include <string>

#include "bitboard.h"
#include "move.h"
#include "generated_data.h"

struct Position;
struct Game;

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

static inline std::string MoveString(Move m)
{
    std::string result;
    result += FileChar(MoveFrom(m));
    result += RankChar(MoveFrom(m));
    result += FileChar(MoveTo(m));
    result += RankChar(MoveTo(m));
    if (MovePromoted(m))
    {
        result += "  nbrq"[MovePromoted(m)];
    }
    return result;
}

int     GetMilliseconds(void);
int     EvaluateStaticExchange(const Position& src_position, Move move);
void    InitializeGoodMoveCounts(void);
void    RecordGoodMove(int ply, Move move);
void    ScoreAndSortMoves(const Position& position, MoveList& moves, int ply, int depth);
void    SortMoves(MoveList& moves, bool is_stable_sort);
int     EvaluatePosition(const Position& position, int alpha, int beta);
bool    RunMergeSortTests(void);
void    RunPerftTests(void);
void    RunPositionTests(Game& game, int depth);
void    RunStaticExchangeTests(void);
int     NextRandom(void);
void    ProcessInput(Game& game, char* line);