#include "pawnstar.h"
/*
Determine if the current position represents a draw by repetition.

We use position Zobrist hash values to determine position equality - there is a
tiny chance of hash collision causing an error - I have not seen it happen 
in practice.
*/
bool IsDrawByRepetition(const Position& p, bool is_search)
{
    const Position* position = &p;
    const uint64_t hash = p.hash_;
    int repetitions = is_search ? 1 : 2;
    for (position = position->previous_; 
         position->reversible_move_count_ != 0; 
         position = position->previous_)
    {
        if (position->hash_ == hash && --repetitions == 0)
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
bool IsDrawByFiftyMoves(const Position& position)
{
    if (position.reversible_move_count_ >= 100)
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
bool IsDrawByMaterial(const Position& position)
{
    const Bitboard occupied_squares = position.white_pieces_ | position.black_pieces_;
    switch (PopCount(occupied_squares))
    {
    case 0:
    case 1:
        printf("ERROR: too few pieces for play\n");
        return true;
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
        if (position.bishops_ | position.knights_)
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
            const Bitboard white_bishops = position.bishops_ & position.white_pieces_;
            const Bitboard black_bishops = position.bishops_ ^ white_bishops;
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
bool IsStalemate(const Position& position)
{
    Move moves[MAX_MOVES_PER_POSITION];
    return 
        !(position.flags_ & IS_CHECK) && 
        GenerateLegalMoves(position, moves) == 0;
}
/*
Determine if the current position represents checkmate
*/
bool IsCheckmate(const Position& position)
{
    Move moves[MAX_MOVES_PER_POSITION];
    return 
        (position.flags_ & IS_CHECK) && 
        GenerateLegalMoves(position, moves) == 0;
}
/*
Determine if the current position state is a legal chess position:
a) each side shall have strictly 1 king
b) kings shall not be adjacent
c) the side not on move shall not be in check
*/
bool IsPositionLegal(const Position& position)
{
    const int color           = ColorToMove(position);
    const Bitboard white_king = position.kings_ & position.white_pieces_;
    const Bitboard black_king = position.kings_ & position.black_pieces_;
    return
        PopCount(white_king) == 1                           &&
        PopCount(black_king) == 1                           &&
        white_king != black_king                            &&
        !(SETS[Lsb(white_king)].king_attacks & black_king)  &&
        !IsAttacked(position, position.king_location_[EnemyOf(color)], color);
}
/*
If the game is over, display the xboard style result string
*/
void DisplayResultIfGameOver(const Position& position)
{
    switch (position.flags_ & IS_GAME_OVER)
    {
    default:
        break;
    case IS_CHECKMATE:
        printf((position.flags_ & IS_BLACK_TO_MOVE) ? "1-0 {white mates}\n" : "0-1 {black mates}\n");
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