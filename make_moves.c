#include "pawnstar.h"

#define IS_IN_CHECK(position, color) (IsAttacked(position, position->king_location[color], ENEMY(color)))

/**
 * @brief Castling rights flags to be cleared based on move from and to squares.
*/
static const uint8 CASTLING_RIGHTS_MASKS[64] =
{
    [E1] = MAY_WHITE_K | MAY_WHITE_Q,
    [A1] = MAY_WHITE_Q,
    [H1] = MAY_WHITE_K,
    [E8] = MAY_BLACK_K | MAY_BLACK_Q,
    [A8] = MAY_BLACK_Q,
    [H8] = MAY_BLACK_K,
};

/**
 * @brief Make a non (null) move
 * - Flip side to move
 * - Clear en passant capture availablity
 * @param dst_position destination position
 * @param src_position source position
*/
void 
MakeNullMove(Position* dst_position, 
             const Position* src_position)
{
    memcpy(dst_position, src_position, sizeof(Position));
    dst_position->previous = src_position;
    dst_position->move = 0;
    if (dst_position->en_passant_index)
    {
        dst_position->hash -= EN_PASSANT_HASHES[FILE_OF(dst_position->en_passant_index)];
        dst_position->en_passant_index = 0;
    }
    dst_position->state_flags ^= IS_BLACK_TO_MOVE;
    dst_position->hash += (dst_position->state_flags & IS_BLACK_TO_MOVE) ? BLACK_MOVE_HASH : -BLACK_MOVE_HASH;
}

void
AddPiece(Position* position, 
         int color, 
         int piece, 
         int to)
{
    position->pieces[piece]          ^= BITBOARD(to);
    position->pieces_of_color[color] ^= BITBOARD(to);
    position->hash  += PIECE_SQUARE_HASHES[color][piece][to];
    position->score += piece_square_values[color][piece][to];
}

void
RemovePiece(Position* position, 
            int color, 
            int piece, 
            int from)
{
    position->pieces[piece]          ^= BITBOARD(from);
    position->pieces_of_color[color] ^= BITBOARD(from);
    position->hash  -= PIECE_SQUARE_HASHES[color][piece][from];
    position->score -= piece_square_values[color][piece][from];
}

void
MovePiece(Position* position, 
          int color, 
          int piece, 
          int from, 
          int to)
{
    position->pieces[piece]          ^= BITBOARD(from) | BITBOARD(to);
    position->pieces_of_color[color] ^= BITBOARD(from) | BITBOARD(to);
    position->hash  += PIECE_SQUARE_HASHES[color][piece][to] - PIECE_SQUARE_HASHES[color][piece][from];
    position->score += piece_square_values[color][piece][to] - piece_square_values[color][piece][from];
}

/**
 * @brief Construct a new position from a source position and a move.
 * @param dst_position destination (new) position
 * @param src_position source (old) position
 * @param move move being made
*/
void 
MakeMove(Position*       dst_position, 
         const Position* src_position, 
         int             move)
{                          
    const int color    = COLOR_TO_MOVE(src_position);
    const int from     = MOVE_FROM(move);
    const int to       = MOVE_TO(move);
    const int piece    = MOVE_PIECE(move);
    const int captured = MOVE_CAPTURED(move);
     
    *dst_position = *src_position;
    dst_position->previous = src_position;
    dst_position->move = move;
    dst_position->castle_flags &= ~CASTLING_RIGHTS_MASKS[from] & ~CASTLING_RIGHTS_MASKS[to];
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
        if (captured)
        {
            if (MOVE_IS_SPECIAL(move))
            {
                // En passant capture: capture location is source rank, destination file
                const int en_passant_capture_location = (from & 0x38) | (to & 0x07);
                RemovePiece(dst_position, ENEMY(color), PAWN, en_passant_capture_location);
            }
            else
            {
                RemovePiece(dst_position, ENEMY(color), captured, to);
            }
        }                
        else if (((to - from) & 0xF) == 0)
        {
            // Pawn double push: affects en passant.
            dst_position->en_passant_index = (uint8)((from + to) >> 1);
            dst_position->hash += EN_PASSANT_HASHES[FILE_OF(from)];
        }       
        if (MOVE_PROMOTED(move))
        {
            // Pawn promotion.
            RemovePiece(dst_position, color, PAWN, from);
            AddPiece(dst_position, color, MOVE_PROMOTED(move), to);
        }
        else
        {
            MovePiece(dst_position, color, PAWN, from, to);
        }
        break;

    RegularMove:
    default:
    case KNIGHT:
    case BISHOP:
    case ROOK:
    case QUEEN:
        if (captured)
        {
            RemovePiece(dst_position, ENEMY(color), captured, to);
            dst_position->reversible_move_count = 0;
        }
        MovePiece(dst_position, color, piece, from, to);
        break;

    case KING:
        dst_position->king_location[color] = (uint8)to;
        if (!MOVE_IS_SPECIAL(move))
        {
            goto RegularMove;
        }
        // Castling move.
        switch (to)
        {
        case G1:
            MovePiece(dst_position, WHITE, KING, E1, G1);
            MovePiece(dst_position, WHITE, ROOK, H1, F1);
            break;
        case C1:
            MovePiece(dst_position, WHITE, KING, E1, C1);
            MovePiece(dst_position, WHITE, ROOK, A1, D1);
            break;
        case G8:
            MovePiece(dst_position, BLACK, KING, E8, G8);
            MovePiece(dst_position, BLACK, ROOK, H8, F8);
            break;
        case C8:
            MovePiece(dst_position, BLACK, KING, E8, C8);
            MovePiece(dst_position, BLACK, ROOK, A8, D8);
            break;
        default:
            break;
        }
        break;
    }
    dst_position->occupied_squares = dst_position->white_pieces | dst_position->black_pieces;  
    dst_position->state_flags ^= IS_BLACK_TO_MOVE;
    dst_position->hash += (dst_position->state_flags & IS_BLACK_TO_MOVE) ? BLACK_MOVE_HASH : -BLACK_MOVE_HASH;
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
/*
Make a move and update the game end status flags
*/
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
/*
Play a move from the given move string in either standard algebraic or XBoard 
format, and update game state_flags accordingly

returns the move if a legal move was played
returns zero if the move was illegal or the game is over
*/
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
