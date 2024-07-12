#pragma once

#include <string>
#include <string_view>

#include "bitboard.h"
#include "generated_data.h"
#include "move.h"

class Position;
struct Game;

constexpr uint8_t FileOf(Square locn)
{
    return locn & 7;
}
constexpr uint8_t RankOf(Square locn)
{
    return locn >> 3;
}
constexpr char FileChar(Square locn)
{
    return 'a' + FileOf(locn);
}
constexpr char RankChar(Square locn)
{
    return '1' + RankOf(locn);
}
constexpr Color EnemyOf(Color color)
{
    return color == WHITE ? BLACK : WHITE;
}
constexpr int min(int a, int b)
{
    return a < b ? a : b;
}
constexpr int max(int a, int b)
{
    return a > b ? a : b;
}

constexpr Bitboard BishopAttacks(Bitboard occupied_squares, Square location)
{
    const MagicMoveEntry &m = BISHOP_MAGICS[location];
    return m.attacks[m.indices[((occupied_squares & m.occupancy_mask) * m.magic) >> m.shift]];
}

constexpr Bitboard RookAttacks(Bitboard occupied_squares, Square location)
{
    const MagicMoveEntry &m = ROOK_MAGICS[location];
    return m.attacks[m.indices[((occupied_squares & m.occupancy_mask) * m.magic) >> m.shift]];
}

constexpr Bitboard QueenAttacks(Bitboard occupied_squares, Square location)
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

int  GetMilliseconds(void);
int  EvaluateStaticExchange(const Position &src_position, Move move, bool &is_checking);
void RecordKillerMove(int ply, Move move);
void ResetKillerCounts();
void ScoreAndSortMoves(const Position &position, MoveList &moves, int ply, int depth);
void SortMoves(MoveList &moves, bool is_stable_sort);
int  EvaluatePosition(const Position &position, int alpha, int beta);
bool RunMergeSortTests(void);
void RunPerftTests(void);
void RunPositionTests(int depth);
void RunStaticExchangeTests(void);
int  NextRandom(void);
void ProcessInput(Game &game, std::string_view line);