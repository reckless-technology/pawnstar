#pragma once
#include <stdio.h>
#include <string>

#include "bitboard.h"
#include "types.h"

constexpr uint8_t   FileOf(int locn)                        { return locn & 7;              }
constexpr uint8_t   RankOf(int locn)                        { return locn >> 3;             }
constexpr char      FileChar(int locn)                      { return 'a' + FileOf(locn);    }
constexpr char      RankChar(int locn)                      { return '1' + RankOf(locn);    }
constexpr uint8_t   EnemyOf(int color)                      { return !color;                }
constexpr int       min(int a, int b)                       { return a < b ? a : b;         }
constexpr int       max(int a, int b)                       { return a > b ? a : b;         }
constexpr uint8_t   ColorToMove(const Position& position)   { return position.flags & IS_BLACK_TO_MOVE ? BLACK : WHITE; }

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
        (square & position.pawns)   ? PAWN   :
        (square & position.knights) ? KNIGHT :
        (square & position.bishops) ? BISHOP :
        (square & position.rooks)   ? ROOK   :
        (square & position.queens)  ? QUEEN  :
        (square & position.kings)   ? KING   : NO_PIECE;
}

/*
Search
*/
int         Search          (const Position& position, int depth, int ply, int alpha, int beta, volatile bool& cancel, Variation& parent_pv);
int         SearchQuiescent (const Position& position, int depth, int ply, int alpha, int beta, volatile bool& cancel);
Move        SearchRootNode  (const Position& position);
int         SearchSingleMove(const Position& position, int depth, int ply, int alpha, int beta, volatile bool& cancel, Move move, Variation& pv, int move_index);
/*
Thinking and time control
*/
int         GetMilliseconds(void);
void        StartThinking(Game& game);
void        StopThinking(void);
void        StopWorker(void);
/*
Support for FEN and SAN strings
*/
bool        AreMoveStringsEquivalent(char* str1, char* str2);
uint64_t    ComputeHash(const Position& position);
void        InitializeGame(Game& game);
bool        IsPositionLegal(const Position& position);
Move        PlayMoveString(Game& game, char* move_string);
int         MoveSequenceToSanString(const Position& position, const Move* moves, char* move_string);
int         MoveToString(const Position& position, Move move, char* move_string);
/*
Attacks
*/
Bitboard    AttacksTo    (const Position& position, int location, int color);
bool        IsAttacked   (const Position& position, int location, int color);
Bitboard    BishopAttacks(Bitboard occupied_squares, int location);
Bitboard    QueenAttacks (Bitboard occupied_squares, int location);
Bitboard    RookAttacks  (Bitboard occupied_squares, int location);
/*
Opening book
*/
void        DisplayAvailableBookMoves(const Position& position);
void        FreeOpeningBook(void);
Move        GetBookMove(uint64_t hash);
bool        InitializeOpeningBookFromFile(const char* filename);
bool        InitializeDefaultOpeningBook();
/*
Transposition table
*/
bool        FindTransposition(uint64_t hash, Transposition* transposition);
void        FreeTranspositionTable(void);
bool        InitializeTranspositionTable(int megabytes);
void        RecordTransposition(uint64_t hash, int depth, int score, Move move, int node_type);
/*
Generation and ordering of moves
*/
void        DeterminePins(const Position& position, Pins& pins);
int         EvaluateStaticExchange(const Position& src_position, Move move);
void        InitializeGoodMoveCounts(void);
void        RecordGoodMove(int ply, Move move);
int         GenerateLegalMoves(const Position& position, Move moves[]);
void        GeneratePseudoLegalCaptures(const Position& position, Move moves[]);
void        GeneratePseudoLegalMoves(const Position& position, Move moves[]);
void        ScoreAndSortMoves(const Position& position, Move moves[], int ply, int depth);
void        SortMoves(int num_elements, Move values[]);
/*
Positional evaluation
*/
int         EvaluatePosition(const Position& position, int alpha, int beta);
/*
Game over (terminal node) detection
*/
void        DisplayResultIfGameOver(const Position& position);
bool        IsCheckmate(const Position& position);
bool        IsDrawByFiftyMoves(const Position& position);
bool        IsDrawByMaterial(const Position& position);
bool        IsDrawByRepetition(const Position& position, bool is_search);
bool        IsStalemate(const Position& position);
/*
Making moves
*/
void        AddPiece(Position& position, int color, int piece, int to);
void        MakeMove(Position& dst_position, const Position& src_position, Move move);
void        MakeMoveSequence(Position& dst_position, const Position& src_position, const Move* moves);
void        MakeNullMove(Position& dst_position, const Position& src_position);
void        PlayMove(Game& game, Move move);
/*
Tests
*/
bool        RunMergeSortTests(void);
void        RunPerftTests(void);
void        RunPositionTests(int depth);
void        RunStaticExchangeTests(void);
/*
Simple PRNG
*/
int         NextRandom(void);
/*
Process Input Commands
*/
void        ProcessInput(char* line);

/*
Debugging counts dictionary - DEBUG and RELEASEX configurations only
*/
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
