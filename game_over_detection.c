#include "pawnstar.h"
/*
Determine if the current position represents a draw by repetition.

We use position Zobrist hash values to determine position equality - there is a
tiny chance of hash collision causing an error - I have not seen it happen 
in practice.
*/
bool IsDrawByRepetition(const Position* position, bool is_search)
{
    const uint64 hash = position->hash;
    int repetitions = is_search ? 1 : 2;
    for (int i = position->reversible_move_count; i != 0; --i)
    {
        position = position->previous;
        if (position->hash == hash && --repetitions == 0)
        {
            INCREMENT("draws by repetition");
            return true;
        }
    }
    return false;
}
/*
Determine if the current position represents a draw by the 50 move rule
(50 consecutive reversible moves by each player is a drawn game)
*/
bool IsDrawByFiftyMoves(const Position* position)
{
    if (position->reversible_move_count >= 100)
    {
        INCREMENT("draws by 50 moves");
        return true;
    }
    return false;
}
/*
Determine if the current position represents a draw by insufficient material

according to FIDE rules the drawn combinations are:

a) king vs king
b) king and knight vs king
c) king and bishop vs king
d) king and bishop vs king and bishop, with bishops on the same color square
*/
bool IsDrawByMaterial(const Position* position)
{
    switch (PopCount(position->occupied_squares))
    {
    case 0:
    case 1:
        printf("ERROR: too few pieces for play\n");
    case 2:
        /*
        king vs king
        */
        INCREMENT("draws by material (2)");
        return true;
    case 3:
        /*
        king and bishop vs king
        king and knight vs king
        */
        if (position->bishops | position->knights)
        {
            INCREMENT("draws by material (3)");
            return true;
        }
        return false;       
    case 4:
        /*
        king and bishop vs king and bishop with bishops on same color square
        */
        {
            const bitboard white_bishops = position->bishops & position->white_pieces;
            const bitboard black_bishops = position->bishops ^ white_bishops;
            if (white_bishops && black_bishops && (!(white_bishops & WHITE_SQUARES) == !(black_bishops & WHITE_SQUARES)))
            {
                INCREMENT("draws by material (4)");
                return true;
            }
            return false;
        }
    default:
        return false;
    }
}
/*
Determine if the current position represents stalemate
*/
bool IsStalemate(const Position* position)
{
    int moves[MAX_MOVES_PER_POSITION];
    return 
        !(position->state_flags & IS_CHECK) && 
        GenerateLegalMoves(position, moves) == 0;
}
/*
Determine if the current position represents checkmate
*/
bool IsCheckmate(const Position* position)
{
    int moves[MAX_MOVES_PER_POSITION];
    return 
        (position->state_flags & IS_CHECK) && 
        GenerateLegalMoves(position, moves) == 0;
}
/*
Determine if the current position state is a legal chess position:
a) each side shall have strictly 1 king
b) kings shall not be adjacent
c) the side not on move shall not be in check
*/
bool IsPositionLegal(const Position* position)
{
    const int color           = COLOR_TO_MOVE(position);
    const bitboard white_king = position->kings & position->white_pieces;
    const bitboard black_king = position->kings & position->black_pieces;
    return
        PopCount(white_king) == 1                     &&
        PopCount(black_king) == 1                     &&
        white_king != black_king                      &&
        !(KING_ATTACKS[Lsb(white_king)] & black_king) &&
        !IsAttacked(position, position->king_location[ENEMY(color)], color);
}
/*
If the game is over, display the xboard style result string
*/
void DisplayResultIfGameOver(const Position* position)
{
    switch (position->state_flags & IS_GAME_OVER)
    {
    default:
        break;
    case IS_CHECKMATE:
        printf((position->state_flags & IS_BLACK_TO_MOVE) ? "1-0 {white mates}\n" : "0-1 {black mates}\n");
        break;
    case IS_STALEMATE:
        printf("1/2-1/2 {stalemate}\n");
        break;
    case IS_DRAW_REPETITION:
        printf("1/2-1/2 {draw by repetition}\n");
        break;
    case IS_DRAW_MATERIAL:
        printf("1/2-1/2 {draw by insufficient material}\n");
        break;
    case IS_DRAW_50_MOVES:
        printf("1/2-1/2 {draw by 50 reversible moves}\n");
        break;
    }
}