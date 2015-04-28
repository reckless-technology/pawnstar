#include "pawnstar.h"

#define IS_IN_CHECK(position, color) (IsAttacked(position, position->king_location[color], ENEMY(color)))
/******************************************************************************
Castling rights masks are ANDed with the castle_flags for the move source and
destination squares to determine new castling rights following a move

Only moves involving squares a1, e1, h1, a8, e8 or h8 affect castling rights
*******************************************************************************/
static const uchar CASTLING_RIGHTS_MASKS[64] =
{
    0xFD, 0xFF, 0xFF, 0xFF, 0xFC, 0xFF, 0xFF, 0xFE, // 1
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // 2
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // 3
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // 4
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // 5
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // 6
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // 7
    0xF7, 0xFF, 0xFF, 0xFF, 0xF3, 0xFF, 0xFF, 0xFB, // 8
};//  a     b     c     d     e     f     g     h
/******************************************************************************
Make a "null" move (used for null move pruning during search)
*******************************************************************************/
void MakeNullMove(Position* dst_position, const Position* src_position)
{
    *dst_position = *src_position;
    dst_position->previous = src_position;
    dst_position->move = 0;
    if (dst_position->en_passant_index)
    {
        dst_position->hash -= EN_PASSANT_HASHES[FILE_OF(dst_position->en_passant_index)];
        dst_position->en_passant_index = 0;
    }
    dst_position->state_flags ^= IS_BLACK_TO_MOVE;
    if (dst_position->state_flags & IS_BLACK_TO_MOVE)
    {
        dst_position->hash += BLACK_MOVE_HASH;
    }
    else
    {
        dst_position->hash -= BLACK_MOVE_HASH;
    }
}
/******************************************************************************
Construct a new position from an old position and a move
*******************************************************************************/
void MakeMove(Position* dst_position, const Position* src_position, int move)
{                          
    const int color             = COLOR_TO_MOVE(src_position);
    const int from              = MOVE_FROM(move);
    const int to                = MOVE_TO(move);
    const int piece             = MOVE_PIECE(move);
    const int captured          = MOVE_CAPTURED(move);
    const int promoted          = MOVE_PROMOTED(move); 
    const bitboard from_bb      = BITBOARD(from);
    const bitboard to_bb        = BITBOARD(to);
    const bitboard from_to_bb   = from_bb | to_bb;
    
    *dst_position = *src_position;
    dst_position->previous = src_position;
    dst_position->move = move;
    dst_position->castle_flags &= CASTLING_RIGHTS_MASKS[from] & CASTLING_RIGHTS_MASKS[to];
    if (dst_position->castle_flags != src_position->castle_flags)
    {
        dst_position->hash += CASTLING_RIGHTS_HASHES[dst_position->castle_flags] - CASTLING_RIGHTS_HASHES[src_position->castle_flags];
    }
    if (dst_position->en_passant_index)
    {
        dst_position->hash -= EN_PASSANT_HASHES[FILE_OF(dst_position->en_passant_index)];
        dst_position->en_passant_index = 0;
    }
    ++dst_position->reversible_move_count;
    switch (piece)
    {
    case PAWN:
        dst_position->reversible_move_count = 0;
        if (!promoted)
        {
            if (!MOVE_IS_SPECIAL(move))
            {
                /* regular pawn move */
                dst_position->pawns ^= from_to_bb;
                dst_position->pieces_of_color[color] ^= from_to_bb;
                dst_position->hash += PIECE_SQUARE_HASHES[color][PAWN][to] - PIECE_SQUARE_HASHES[color][PAWN][from];
                if (captured)
                {
                    dst_position->pieces[captured] ^= to_bb;
                    dst_position->pieces_of_color[ENEMY(color)] ^= to_bb;
                    dst_position->hash -= PIECE_SQUARE_HASHES[ENEMY(color)][captured][to];
                }
                else if (!((to - from) & 0x0F)) /* equivalent to (abs(from - to) == 16) in this context */
                {
                    /* double pawn push */
                    dst_position->en_passant_index = (uchar)((from + to) >> 1);
                    dst_position->hash += EN_PASSANT_HASHES[FILE_OF(from)];
                }
            }
            else
            {
                /* en passant capture: capture location is source rank, destination file */
                const int en_passant_capture_location = (from & 0x38) | (to & 0x07);
                const bitboard en_passant_capture_BB = BITBOARD(en_passant_capture_location);       
                dst_position->pieces_of_color[color] ^= from_to_bb;
                dst_position->pieces_of_color[ENEMY(color)] ^= en_passant_capture_BB;
                dst_position->pawns ^= from_to_bb | en_passant_capture_BB;
                dst_position->hash += PIECE_SQUARE_HASHES[color][PAWN][to] - PIECE_SQUARE_HASHES[color][PAWN][from] - PIECE_SQUARE_HASHES[ENEMY(color)][PAWN][en_passant_capture_location];
            }
        }
        else
        {
            /* pawn promotion */
            dst_position->pawns ^= from_bb;
            dst_position->pieces[promoted] ^= to_bb;
            dst_position->pieces_of_color[color] ^= from_to_bb;
            dst_position->hash += PIECE_SQUARE_HASHES[color][promoted][to] - PIECE_SQUARE_HASHES[color][PAWN][from];
            if (captured)
            {
                dst_position->pieces[captured] ^= to_bb;
                dst_position->pieces_of_color[ENEMY(color)] ^= to_bb;
                dst_position->hash -= PIECE_SQUARE_HASHES[ENEMY(color)][captured][to];
            }
        }
        break;

    RegularMove:
    default:
    case KNIGHT:
    case BISHOP:
    case ROOK:
    case QUEEN:
        dst_position->pieces[piece] ^= from_to_bb;
        dst_position->pieces_of_color[color] ^= from_to_bb;
        dst_position->hash += PIECE_SQUARE_HASHES[color][piece][to] - PIECE_SQUARE_HASHES[color][piece][from];
        if (captured)
        {
            dst_position->reversible_move_count = 0;
            dst_position->pieces[captured] ^= to_bb;
            dst_position->pieces_of_color[ENEMY(color)] ^= to_bb;
            dst_position->hash -= PIECE_SQUARE_HASHES[ENEMY(color)][captured][to];
        }
        break;

    case KING:
        dst_position->king_location[color] = (uchar)to;
        if (!MOVE_IS_SPECIAL(move))
        {
            goto RegularMove;
        }
        /* castling move */
        switch (to)
        {
        case G1:
            dst_position->kings ^= E1BB | G1BB;
            dst_position->rooks ^= H1BB | F1BB;
            dst_position->white_pieces ^= E1BB | F1BB | G1BB | H1BB;
            dst_position->hash += PIECE_SQUARE_HASHES[WHITE][KING][G1] - PIECE_SQUARE_HASHES[WHITE][KING][E1] + PIECE_SQUARE_HASHES[WHITE][ROOK][F1] - PIECE_SQUARE_HASHES[WHITE][ROOK][H1];
            break;
        case C1:
            dst_position->kings ^= E1BB | C1BB;
            dst_position->rooks ^= A1BB | D1BB;
            dst_position->white_pieces ^= A1BB | C1BB | D1BB | E1BB;
            dst_position->hash += PIECE_SQUARE_HASHES[WHITE][KING][C1] - PIECE_SQUARE_HASHES[WHITE][KING][E1] + PIECE_SQUARE_HASHES[WHITE][ROOK][D1] - PIECE_SQUARE_HASHES[WHITE][ROOK][A1];
            break;
        case G8:
            dst_position->kings ^= E8BB | G8BB;
            dst_position->rooks ^= H8BB | F8BB;
            dst_position->black_pieces ^= E8BB | F8BB | G8BB | H8BB;
            dst_position->hash += PIECE_SQUARE_HASHES[BLACK][KING][G8] - PIECE_SQUARE_HASHES[BLACK][KING][E8] + PIECE_SQUARE_HASHES[BLACK][ROOK][F8] - PIECE_SQUARE_HASHES[BLACK][ROOK][H8];
            break;
        case C8:
            dst_position->kings ^= E8BB | C8BB;
            dst_position->rooks ^= A8BB | D8BB;
            dst_position->black_pieces ^= A8BB | C8BB | D8BB | E8BB;
            dst_position->hash += PIECE_SQUARE_HASHES[BLACK][KING][C8] - PIECE_SQUARE_HASHES[BLACK][KING][E8] + PIECE_SQUARE_HASHES[BLACK][ROOK][D8] - PIECE_SQUARE_HASHES[BLACK][ROOK][A8];
            break;
        default:
            break;
            }
        break;
    }
    dst_position->occupied_squares = dst_position->white_pieces | dst_position->black_pieces;  
    dst_position->state_flags ^= IS_BLACK_TO_MOVE;
    if (dst_position->state_flags & IS_BLACK_TO_MOVE)
    {
        dst_position->hash += BLACK_MOVE_HASH;
    }
    else
    {
        dst_position->hash -= BLACK_MOVE_HASH;
    }
    dst_position->full_move_count += (color == BLACK);
    if (IS_IN_CHECK(dst_position, color))
    {
        dst_position->state_flags |= MOVED_INTO_CHECK;
    }
    else
    {
        if (IS_IN_CHECK(dst_position, ENEMY(color)))
        {
            dst_position->state_flags |= IS_CHECK;
        }
        else
        {
            dst_position->state_flags &= ~IS_CHECK;
        }
    }
}
/******************************************************************************
Make a move and update the game end status flags
*******************************************************************************/
void PlayMove(Game* game, int move)
{
    if (game->position->state_flags & IS_GAME_OVER)
    {
        printf("ERROR: attempt to play move after game is over\n");
        return;
    }
    MakeMove(game->position + 1, game->position, move);
    ++game->position;
    if (IsCheckmate(game->position))
    {
        game->position->state_flags |= IS_CHECKMATE;
    }
    else if (IsStalemate(game->position))
    {
        game->position->state_flags |= IS_STALEMATE;
    }
    else if (IsDrawByRepetition(game->position, false))
    {
        game->position->state_flags |= IS_DRAW_REPETITION;
    }
    else if (IsDrawByMaterial(game->position))
    {
        game->position->state_flags |= IS_DRAW_MATERIAL;
    }
    else if (IsDrawByFiftyMoves(game->position))
    {
        game->position->state_flags |= IS_DRAW_50_MOVES;
    }
}
/******************************************************************************
Play a move from the given move string in either standard algebraic or XBoard 
format, and update game state_flags accordingly

returns the move if a legal move was played
returns zero if the move was illegal or the game is over
*******************************************************************************/
int PlayMoveString(Game* game, char* move_str, bool is_san)
{
    int moves[MAX_MOVES_PER_POSITION];
    const int* move;
    if (game->position->state_flags & IS_GAME_OVER)
    {
        return 0;
    }    
    GenerateLegalMoves(game->position, moves);
    for (move = moves; *move; ++move)
    {
        char buffer[16];
        if (is_san)
        {
            MoveToSanString(game->position, *move,  buffer);
        }
        else
        {
            MoveToString(*move, buffer);
        }
        if (AreMoveStringsEqual(buffer, move_str))
        {
            PlayMove(game, *move);
            return *move;
        }
    }
    return 0;
}
