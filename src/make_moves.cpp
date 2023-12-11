#include "pawnstar.h"

/*
Make a move and update the game end status flags
*/
void PlayMove(Game& game, Move move)
{
    if (game.position->flags_ & IS_GAME_OVER)
    {
        printf("ERROR: attempt to play move after game is over\n");
        return;
    }
    game.position[1] = Position(game.position[0], move);
    ++game.position;
    if (IsCheckmate(*game.position))
    {
        game.position->flags_ |= IS_CHECKMATE;
    }
    else if (IsStalemate(*game.position))
    {
        game.position->flags_ |= IS_STALEMATE;
    }
    else if (IsDrawByRepetition(*game.position, false))
    {
        game.position->flags_ |= IS_DRAW_REPETITION;
    }
    else if (IsDrawByMaterial(*game.position))
    {
        game.position->flags_ |= IS_DRAW_MATERIAL;
    }
    else if (IsDrawByFiftyMoves(*game.position))
    {
        game.position->flags_ |= IS_DRAW_50_MOVES;
    }
}
/*
Play a move from the given move string in either standard algebraic or XBoard 
format, and update game state_flags accordingly

returns the move if a legal move was played
returns zero if the move was illegal or the game is over
*/
Move PlayMoveString(Game& game, char* move_str)
{
    if (game.position->flags_ & IS_GAME_OVER)
    {
        return 0;
    }  
    Move moves[MAX_MOVES_PER_POSITION];
    GenerateLegalMoves(*game.position, moves);
    for (const Move* move = moves; *move; ++move)
    {
        char buffer[16];
        MoveToString(*game.position, *move,  buffer);
        if (AreMoveStringsEquivalent(buffer, move_str))
        {
            PlayMove(game, *move);
            return *move;
        }
    }
    return 0;
}

/**
 * @brief Make a sequence of moves.
 * @param dst_position destination position after the sequence
 * @param src_position source position
 * @param moves zero terminated list of moves
*/
void MakeMoveSequence(Position& dst_position, const Position& src_position, const Move* moves)
{
    Position tmp = src_position;
    for ( ; *moves; ++moves)
    {
        dst_position = Position(tmp, *moves);
        tmp = dst_position;
    }
    dst_position.previous_ = &dst_position; // Can't assume stack here.
}
