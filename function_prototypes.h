#pragma once
#include <stdio.h>
#include "types.h"

#ifdef __cplusplus 
extern "C" {
#endif
/*
Search
*/
int         SearchQuiescent (const Position* src_position, int depth, int ply, int alpha, int beta, volatile bool* cancel);
int         Search          (const Position* src_position, int depth, int ply, int alpha, int beta, volatile bool* cancel, Variation* parent_pv);
int         SearchSingleMove(const Position* src_position, int depth, int ply, int alpha, int beta, volatile bool* cancel, int move, Variation* pv, int move_index);
int         SearchRootNode  (const Position* position);
/*
Thinking and time control
*/
void        StartThinking(Game* game);
void        StopThinkingMoveImmediately(void);
void        StopWorker(void);
int         GetMilliseconds(void);
/*
Support for FEN and SAN strings
*/
bool        PositionFromString(const char* fen_string, Position* position);
void        PositionToString(const Position* position, char* fen_string);
bool        IsPositionLegal(const Position* position);
void        MoveSequenceToSanString(const Position* position, const int moves[], char* move_string);
char*       MoveToString(const Position* position, int the_move, char* move_string);
int         PlayMoveString(Game* game, char* move_string);
bool        AreMoveStringsEquivalent(char* str1, char* str2);
uint64_t    ComputeHash(const Position* position);
void        InitializeGame(Game* game);
/*
Attacks
*/
bitboard    BishopAttacks(bitboard occupied_squares, int location);
bitboard    RookAttacks  (bitboard occupied_squares, int location);
bitboard    QueenAttacks (bitboard occupied_squares, int location);
bitboard    AttacksFromSquare(const Position* position, int location, int piece);
bool        IsAttacked(const Position* position, int location, int color);
/*
Opening book
*/
bool        InitializeOpeningBookFromString(const char* book_string);
bool        InitializeOpeningBookFromFile(const char* filename);
void        FreeOpeningBook(void);
int         GetBookMove(uint64_t hash);
void        DisplayAvailableBookMoves(const Position* position);
/*
Transposition table
*/
bool        InitializeTranspositionTable(int megabytes);
void        FreeTranspositionTable(void);
void        RecordTransposition(uint64_t hash, int depth, int score, int move, int node_type);
bool        FindTransposition(uint64_t hash, Transposition* transposition);
/*
Generation and ordering of moves
*/
void        InitializeGoodMoveCounts(void);
void        RecordGoodMove(int ply, int move);
int         GenerateLegalMoves(const Position* position, int moves[]);
void        GeneratePseudoLegalMoves(const Position* position, int captures[], int non_captures[]);
int         EvaluateStaticExchange(const Position* src_position, int move);
void        SortMoves(const Position* position, int moves[], int ply, bool use_see);
void        MergeSort(int num_elements, ScoredMove values[]);
/*
Positional evaluation
*/
void        InitializeEval(void);
int         EvaluatePosition(const Position* position, int alpha, int beta);
/*
Game over (terminal node) detection
*/
bool        IsCheckmate(const Position* position);
bool        IsDrawByFiftyMoves(const Position* position);
bool        IsDrawByMaterial(const Position* position);
bool        IsDrawByRepetition(const Position* position, bool is_search);
bool        IsStalemate(const Position* position);
void        DisplayResultIfGameOver(const Position* position);
/*
Making moves
*/
void        AddPiece(Position* position, int color, int piece, int to);
void        MakeMove(Position* dst_position, const Position* src_position, int move);
void        MakeMoveSequence(Position* dst_position, const Position* src_position, const int* moves);
void        MakeNullMove(Position* dst_position, const Position* src_position);
void        PlayMove(Game* game, int move);
/*
Tests
*/
void        RunPerftTests(void);
void        RunPositionTests(int depth);
void        RunStaticExchangeTests(void);
bool        RunMergeSortTests(void);
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

#ifdef __cplusplus 
}
#endif
