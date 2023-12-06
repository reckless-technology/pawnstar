#include "pawnstar.h"
/*
Functions to generate pseudo-legal and strictly-legal moves. Pseudo-legal moves
may leave our king in check; this is tested during search for improved
efficiency.

Moves are contained within the least significant 22 bits of an integer

  Bits      Interpretation

 0 -  5     to (destination square index)
 6 - 11     from (source square index)
12 - 14     moving piece type
15 - 17     captured piece type in the case of capture moves
18 - 20     promoted piece type in the case of pawn promotions
21 - 21     special flag (castling or en passant capture move)

A value of 0 terminates a move list

Given a first pawn promotion move to queen, generate the under promotions to
knight, bishop and rook
*/
INLINE void GenerateUnderpromotions(int **pmoves)
{
    int *const moves = *pmoves;
    const int move = *moves;
    moves[1] = move ^ ((ROOK ^ QUEEN) << 18);
    moves[2] = move ^ ((BISHOP ^ QUEEN) << 18);
    moves[3] = move ^ ((KNIGHT ^ QUEEN) << 18);
    *pmoves += 4;
}

#define GENERATE_NON_CAPTURES 1

void GeneratePseudoLegalMoves(const Position* position,
                              int             captures[],
                              int             non_captures[])
#include "move_generation_template.h"

#undef GENERATE_NON_CAPTURES
#define GENERATE_NON_CAPTURES 0

void GeneratePseudoLegalCaptures(const Position* position,
                                 int             captures[])
#include "move_generation_template.h"

#undef GENERATE_NON_CAPTURES

/*
Generate all strictly legal moves for this position
This is relatively slow and is not used at each node of the search, since each
move has to be tested to see if it leaves the king in check
Returns the number of moves generated
*/
int GenerateLegalMoves(const Position *position, int moves[])
{
    const int *const initial_ptr = moves;
    const int *move;
    int captures[MAX_MOVES_PER_POSITION];
    int non_captures[MAX_MOVES_PER_POSITION];
    int *phases[] = {captures, non_captures, NULL};
    Position dst_position[1];
    GeneratePseudoLegalMoves(position, captures, non_captures);
    for (int **phase = phases; *phase; ++phase)
    {
        for (move = *phase; *move; ++move)
        {
            MakeMove(dst_position, position, *move);
            if (!(dst_position->state_flags & IS_MOVED_INTO_CHECK))
            {
                *moves++ = *move;
            }
        }
    }
    *moves = 0;
    return (int)(moves - initial_ptr);
}
