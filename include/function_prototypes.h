#pragma once
#include <stdio.h>

#include "bitboard.h"
#include "types.h"

constexpr int  FILE_OF(int locn)                            { return locn & 7;                      }
constexpr int  RANK_OF(int locn)                            { return locn >> 3;                     }
constexpr char FILE_CHAR(int locn)                          { return (char)('a' + FILE_OF(locn));   }
constexpr char RANK_CHAR(int locn)                          { return (char)('1' + RANK_OF(locn));   }
constexpr int  ENEMY(int color)                             { return !color;                        }
constexpr int  COLOR_TO_MOVE(const Position* position)      { return position->state_flags & IS_BLACK_TO_MOVE ? BLACK : WHITE; }
constexpr int  COLOR_NOT_TO_MOVE(const Position* position)  { return position->state_flags & IS_BLACK_TO_MOVE ? WHITE : BLACK; }

constexpr void 
CopyVariation(Variation*       dst_variation, 
              const Variation* src_variation, 
              Move             best_move)
{
    if (dst_variation)
    {
        Move* dst = dst_variation->moves;
        *dst++ = best_move;
        if (src_variation)
        {
            const Move* src = src_variation->moves;
            while (*src)
            {
                *dst++ = *src++;
            }
        }
        *dst = 0;
    }
}

constexpr int
PieceAt(const Position* position, 
        int             location)
{
    const Bitboard square = BITBOARD(location);
    return 
        (square & position->pawns)   ? PAWN   :
        (square & position->knights) ? KNIGHT :
        (square & position->bishops) ? BISHOP :
        (square & position->rooks)   ? ROOK   :
        (square & position->queens)  ? QUEEN  :
        (square & position->kings)   ? KING   : NO_PIECE;
}

/*
Search
*/
int         Search          (const Position* src_position, int depth, int ply, int alpha, int beta, volatile bool* cancel, Variation* parent_pv);
int         SearchQuiescent (const Position* src_position, int depth, int ply, int alpha, int beta, volatile bool* cancel);
Move        SearchRootNode  (const Position* position);
int         SearchSingleMove(const Position* src_position, int depth, int ply, int alpha, int beta, volatile bool* cancel, Move move, Variation* pv, int move_index);
/*
Thinking and time control
*/
int         GetMilliseconds(void);
void        StartThinking(Game* game);
void        StopThinking(void);
void        StopWorker(void);
/*
Support for FEN and SAN strings
*/
bool        AreMoveStringsEquivalent(char* str1, char* str2);
uint64_t    ComputeHash(const Position* position);
void        InitializeGame(Game* game);
bool        IsPositionLegal(const Position* position);
int         PlayMoveString(Game* game, char* move_string);
bool        PositionFromString(const char* fen_string, Position* position);
void        PositionToString(const Position* position, char* fen_string);
void        MoveSequenceToSanString(const Position* position, const Move moves[], char* move_string);
char*       MoveToString(const Position* position, Move the_move, char* move_string);
/*
Attacks
*/
Bitboard    AttacksTo    (const Position* position, int location, int color);
bool        IsAttacked   (const Position* position, int location, int color);
Bitboard    BishopAttacks(Bitboard occupied_squares, int location);
Bitboard    QueenAttacks (Bitboard occupied_squares, int location);
Bitboard    RookAttacks  (Bitboard occupied_squares, int location);
/*
Opening book
*/
void        DisplayAvailableBookMoves(const Position* position);
void        FreeOpeningBook(void);
Move        GetBookMove(uint64_t hash);
bool        InitializeOpeningBookFromFile(const char* filename);
bool        InitializeOpeningBookFromString(const char* book_string);
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
void        DeterminePins(const Position* position, Pins* pins);
int         EvaluateStaticExchange(const Position* src_position, Move move);
void        InitializeGoodMoveCounts(void);
void        RecordGoodMove(int ply, Move move);
int         GenerateLegalMoves(const Position* position, Move moves[]);
void        GeneratePseudoLegalCaptures(const Position* position, Move moves[]);
void        GeneratePseudoLegalMoves(const Position* position, Move moves[]);
void        ScoreAndSortMoves(const Position* position, Move moves[], int ply, int depth);
void        SortMoves(int num_elements, Move values[]);
/*
Positional evaluation
*/
int         EvaluatePosition(const Position* position, int alpha, int beta);
/*
Game over (terminal node) detection
*/
void        DisplayResultIfGameOver(const Position* position);
bool        IsCheckmate(const Position* position);
bool        IsDrawByFiftyMoves(const Position* position);
bool        IsDrawByMaterial(const Position* position);
bool        IsDrawByRepetition(const Position* position, bool is_search);
bool        IsStalemate(const Position* position);
/*
Making moves
*/
void        AddPiece(Position* position, int color, int piece, int to);
void        MakeMove(Position* dst_position, const Position* src_position, Move move);
void        MakeMoveSequence(Position* dst_position, const Position* src_position, const Move* moves);
void        MakeNullMove(Position* dst_position, const Position* src_position);
void        PlayMove(Game* game, Move move);
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
Debugging counts dictionary - DEBUG and RELEASEX configurations only
*/
#if DEBUGX
void        DebugXClear(void);
void        DebugXIncrement(const char* key);
void        DebugXIncrementIf(bool condition, const char* key);
void        DebugXWrite(FILE* file);
#endif
/*
Process Input Commands
*/
void        ProcessInput(char* line);