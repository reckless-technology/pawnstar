#pragma once
#include <stdio.h>
#include "types.h"
/******************************************************************************
Search task management
*******************************************************************************/
void        FreeAllSearchTasks(void);
void        InitializeTaskList(void);
SearchTask* ObtainTaskFromPool(void);
void        ReturnTaskToPool(SearchTask* task);
void        AddPendingTask(SearchTask* task);
SearchTask* GetNextPendingTask(void);
int         GetNumberOfPendingTasks(void);
void        AbortTask(SearchTask* task);
void        DoSomethingUseful(void);
void        InitializeThreads(int num_cpus);
void        WaitForSearchToComplete(void);
/******************************************************************************
Search
*******************************************************************************/
int         Search          (const Position* src_position, int depth, int ply, int alpha, int beta, volatile bool* cancel, bool is_null_ok, bool is_reduce_ok);
int         SearchQuiescent (const Position* src_position, int depth, int ply, int alpha, int beta, volatile bool* cancel);
int         SearchSingleMove(const Position* src_position, int depth, int ply, int alpha, int beta, int move, 
                             int move_index, bool is_deferred_move, volatile bool* cancel, bool is_null_ok, bool is_reduce_ok);
SearchTask* NewSearchTask   (const Position* src_position, int depth, int ply, int alpha, int beta, int move, int move_index);
int         SearchRootNode  (const Position* position);
/******************************************************************************
Thinking and time control
*******************************************************************************/
void        StartThinking(Game* game);
void        SetStopThinkingTimer(int milliseconds, volatile bool* cancel);
void        CancelStopThinkingTimer(void);
void        StopThinkingMoveImmediately(void);
int         GetMilliseconds(void);
/******************************************************************************
Support for FEN and SAN strings
*******************************************************************************/
bool        PositionFromString(const char* fen_string, Position* position);
void        PositionToString(const Position* position, char* fen_string);
bool        IsPositionLegal(const Position* position);
void        MoveSequenceToSanString(const Position* position, const int moves[], char* move_string);
char*       MoveToSanString(const Position* position, int the_move, char* move_string);
void        MoveToString(int move, char* move_string);
int         PlayMoveString(Game* game, char* move_str, bool is_san);
bool        AreMoveStringsEqual(char* str1, char* str2);
uint64      ComputeHash(const Position* position);
void        InitializeGame(Game* game);
void        NewGame(Position* position);
/******************************************************************************
Attacks
*******************************************************************************/
bitboard    BishopAttacks(bitboard occupied_squares, int location);
bitboard    RookAttacks  (bitboard occupied_squares, int location);
bitboard    QueenAttacks (bitboard occupied_squares, int location);
bitboard    AttacksFromSquare(const Position* position, int location, int piece);
bitboard    AttacksToSquare(const Position* position, int location);
bitboard    AttacksToSquareByColor(const Position* position, int location, int color);
bitboard    AttacksToSquareByType(const Position* position, int location, int color, int piece);
bool        IsAttacked(const Position* position, int location, int color);
void        DetermineAttackMap(Position* position);
void        UpdateAttackMap(Position* position);
/******************************************************************************
Opening book
*******************************************************************************/
bool        InitializeOpeningBookFromString(const char* book_string);
bool        InitializeOpeningBookFromFile(const char* filename);
void        FreeOpeningBook(void);
int         GetBookMove(uint64 hash);
void        DisplayAvailableBookMoves(const Position* position);
/******************************************************************************
Transposition table
*******************************************************************************/
bool        InitializeTranspositionTable(int megabytes);
void        FreeTranspositionTable(void);
void        RecordTransposition(uint64 hash, int depth, int score, int move, int node_type);
bool        FindTransposition(uint64 hash, Transposition* transposition);
/******************************************************************************
Principal variation table
*******************************************************************************/
void        InitializePrincipalVariationTable(void);
void        RecordPrincipalVariationMove(uint64 hash, int move);
void        FindPrincipalVariation(const Position* root_position, int principal_variation[]);
int         GetPrincipalVariationMove(uint64 hash);
/******************************************************************************
Generation and ordering of moves
*******************************************************************************/
void        InitializeGoodMoveCounts(void);
void        RecordGoodMove(int ply, int move);
bool        HasMoveBeenGood(int ply, int move);
int         GenerateLegalMoves(const Position* position, int moves[]);
int*        GeneratePseudoLegalMoves(const Position* position, int moves[], bool do_all_moves);
int         EvaluateStaticExchange(const Position* src_position, int move);
void        SortMoves(int moves[], int ply);
void        MergeSort(int num_elements, ScoredMove values[]);
/******************************************************************************
Positional evaluation
*******************************************************************************/
void        InitializeEval(void);
void        DeterminePins(const Position* position, Pins* pins);
void        DeterminePawnStructure(const Position* position, PawnStructure pawn_struct[2]);
void        DisplayPawnStructure(const Position* position);
int         EvaluatePosition(const Position* position, int alpha, int beta);
/******************************************************************************
Game over (terminal node) detection
*******************************************************************************/
bool        IsCheckmate(const Position* position);
bool        IsDrawByFiftyMoves(const Position* position);
bool        IsDrawByMaterial(const Position* position);
bool        IsDrawByRepetition(const Position* position, bool is_search);
bool        IsStalemate(const Position* position);
void        DisplayResultIfGameOver(const Position* position);
/******************************************************************************
Making moves
*******************************************************************************/
void        MakeMove(Position* dst_position, const Position* src_position, int move);
void        MakeNullMove(Position* dst_position, const Position* src_position);
void        PlayMove(Game* game, int move);
/******************************************************************************
Tests
*******************************************************************************/
void        RunPawnStructureTests(void);
void        RunPerftTests(void);
void        RunPositionTests(int depth);
void        RunStaticExchangeTests(void);
/******************************************************************************
Simple PRNG
*******************************************************************************/
int         NextRandom(void);
/******************************************************************************
Debugging counts dictionary - DEBUG and RELEASEX configurations only
*******************************************************************************/
#if DEBUGX
void        DebugXClear(void);
void        DebugXIncrement(const char* key);
void        DebugXIncrementIf(bool condition, const char* key);
void        DebugXWrite(FILE* file);
#endif
