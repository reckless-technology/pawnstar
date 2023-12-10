#include "pawnstar.h"

#define A1M (uint16_t)(~MAY_WHITE_CASTLE_QUEENSIDE)
#define E1M (uint16_t)(~(MAY_WHITE_CASTLE_QUEENSIDE | MAY_WHITE_CASTLE_KINGSIDE))
#define H1M (uint16_t)(~MAY_WHITE_CASTLE_KINGSIDE)
#define A8M (uint16_t)(~MAY_BLACK_CASTLE_QUEENSIDE)
#define E8M (uint16_t)(~(MAY_BLACK_CASTLE_QUEENSIDE | MAY_BLACK_CASTLE_KINGSIDE))
#define H8M (uint16_t)(~MAY_BLACK_CASTLE_KINGSIDE)
#define OK  0xFFFF

/**
 * @brief Castling rights flags based on move square from and to index.
 * Moves to and from king and rook squares invalidate castling rights.
*/
static const uint16_t CASTLING_RIGHTS_MASKS[64] =
{
   A1M,  OK,  OK,  OK, E1M,  OK,  OK, H1M,
    OK,  OK,  OK,  OK,  OK,  OK,  OK,  OK,
    OK,  OK,  OK,  OK,  OK,  OK,  OK,  OK,
    OK,  OK,  OK,  OK,  OK,  OK,  OK,  OK,
    OK,  OK,  OK,  OK,  OK,  OK,  OK,  OK,
    OK,  OK,  OK,  OK,  OK,  OK,  OK,  OK,
    OK,  OK,  OK,  OK,  OK,  OK,  OK,  OK,
   A8M,  OK,  OK,  OK, E8M,  OK,  OK, H8M,
};

/**
 * @brief Make a null move: don't actually move any pieces on the board.
 * # Flip side to move
 * # Clear en passant capture availablity
 * # Set the flag indicating this position is the result of a null move
 * @param position destination position
 * @param src_position source position
*/
void 
MakeNullMove(Position&       position, 
             const Position& src_position)
{
    position = src_position;
    position.previous = &src_position;
    position.flags |= IS_NULL_MOVE;
    position.flags ^= IS_BLACK_TO_MOVE;
    position.hash ^= EN_PASSANT_HASHES[position.en_passant_index];
    position.hash ^= BLACK_MOVE_HASH;
    position.en_passant_index = 0;
}

void
AddPiece(Position& position, 
         int       color, 
         int       piece, 
         int       to)
{
    const Bitboard to_bb = BITBOARD(to);
    position.piece[piece - 1] ^= to_bb;
    position.pieces_of_color[color] ^= to_bb;
    position.hash ^= PIECE_SQUARE_HASHES[color][piece - 1][to];
}

static void
RemovePiece(Position& position, 
            int       color, 
            int       piece, 
            int       from)
{
    const Bitboard from_bb = BITBOARD(from);
    position.piece[piece - 1] ^= from_bb;
    position.pieces_of_color[color] ^= from_bb;
    position.hash ^= PIECE_SQUARE_HASHES[color][piece - 1][from];
}

static void
MovePiece(Position& position, 
          int       color, 
          int       piece, 
          int       from, 
          int       to)
{
    const Bitboard from_to_bb = BITBOARD(from) | BITBOARD(to);
    const uint64_t* const hash = &PIECE_SQUARE_HASHES[color][piece -1][0];
    position.piece[piece - 1] ^= from_to_bb;
    position.pieces_of_color[color] ^= from_to_bb;
    position.hash ^= hash[to] ^ hash[from];
}

/**
 * @brief Construct a new position from a source position and a move.
 * @param position destination (new) position
 * @param src_position source (old) position
 * @param move move being made
*/
void 
MakeMove(Position&       position, 
         const Position& src_position, 
         Move            move)
{                          
    const int color    = ColorToMove(src_position);
    const int from     = MoveFrom(move);
    const int to       = MoveTo(move);
    const int piece    = MovePiece(move);
    const int captured = MoveCaptured(move);
     
    position = src_position;
    position.previous = &src_position;
    position.flags &= ~IS_NULL_MOVE;
    position.flags &= CASTLING_RIGHTS_MASKS[from] & CASTLING_RIGHTS_MASKS[to];
    position.hash ^= CASTLING_RIGHTS_HASHES[position.flags & CASTLING_RIGHTS_MASK] 
                    ^ CASTLING_RIGHTS_HASHES[src_position.flags & CASTLING_RIGHTS_MASK];
    position.hash ^= EN_PASSANT_HASHES[position.en_passant_index];
    position.en_passant_index = 0;
    ++position.reversible_move_count;
    switch (piece)
    {
    case PAWN:
        position.reversible_move_count = 0;
        if (captured)
        {
            if (IsEpCaptureMove(move))
            {
                /* En passant capture: capture location is source rank, destination file. */
                const int captured_pawn_locn = (from & 0x38) | (to & 0x07);
                RemovePiece(position, EnemyOf(color), PAWN, captured_pawn_locn);
            }
            else
            {
                RemovePiece(position, EnemyOf(color), captured, to);
            }
        }                
        if (MovePromoted(move))
        {
            /* Replace the pawn with the promoted piece. */
            RemovePiece(position, color, PAWN, from);
            AddPiece(position, color, MovePromoted(move), to);
        }
        else
        {
            MovePiece(position, color, PAWN, from, to);
            if (IsPawnDoublePushMove(move))
            {
                /* Pawn double push: affects en passant. */
                position.en_passant_index = (uint8_t)((from + to) >> 1);
                position.hash ^= EN_PASSANT_HASHES[position.en_passant_index];
            } 
        }
        break;

    case KNIGHT:
    case BISHOP:
    case ROOK:
    case QUEEN:
    default:
        if (captured)
        {
            RemovePiece(position, EnemyOf(color), captured, to);
            position.reversible_move_count = 0;
        }
        MovePiece(position, color, piece, from, to);
        break;

    case KING:
        if (!IsCastlingMove(move))
        {
            if (captured)
            {
                RemovePiece(position, EnemyOf(color), captured, to);
                position.reversible_move_count = 0;
            }
            MovePiece(position, color, KING, from, to);
            break;
        }
        /* Castling move. */
        switch (to)
        {
        case G1:
            MovePiece(position, WHITE, KING, E1, G1);
            MovePiece(position, WHITE, ROOK, H1, F1);
            break;
        case C1:
            MovePiece(position, WHITE, KING, E1, C1);
            MovePiece(position, WHITE, ROOK, A1, D1);
            break;
        case G8:
            MovePiece(position, BLACK, KING, E8, G8);
            MovePiece(position, BLACK, ROOK, H8, F8);
            break;
        case C8:
            MovePiece(position, BLACK, KING, E8, C8);
            MovePiece(position, BLACK, ROOK, A8, D8);
            break;
        default:
            break;
        }
        break;
    };  
    position.flags ^= IS_BLACK_TO_MOVE;
    position.hash ^= BLACK_MOVE_HASH;
    position.full_move_count += color;
    position.king_location[WHITE] = Lsb(position.kings & position.white_pieces);
    position.king_location[BLACK] = Lsb(position.kings & position.black_pieces);
    if (IsAttacked(position, position.king_location[color], EnemyOf(color)))
    {
        position.flags |= IS_MOVED_INTO_CHECK;
    }
    else
    {
        if (IsAttacked(position, position.king_location[EnemyOf(color)], color))
        {
            position.flags |= IS_CHECK;
        }
        else
        {
            position.flags &= ~IS_CHECK;
        }
    }
}
/*
Make a move and update the game end status flags
*/
void PlayMove(Game& game, Move move)
{
    if (game.position->flags & IS_GAME_OVER)
    {
        printf("ERROR: attempt to play move after game is over\n");
        return;
    }
    MakeMove(*(game.position + 1), *game.position, move);
    ++game.position;
    if (IsCheckmate(*game.position))
    {
        game.position->flags |= IS_CHECKMATE;
    }
    else if (IsStalemate(*game.position))
    {
        game.position->flags |= IS_STALEMATE;
    }
    else if (IsDrawByRepetition(*game.position, false))
    {
        game.position->flags |= IS_DRAW_REPETITION;
    }
    else if (IsDrawByMaterial(*game.position))
    {
        game.position->flags |= IS_DRAW_MATERIAL;
    }
    else if (IsDrawByFiftyMoves(*game.position))
    {
        game.position->flags |= IS_DRAW_50_MOVES;
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
    if (game.position->flags & IS_GAME_OVER)
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
        MakeMove(dst_position, tmp, *moves);
        tmp = dst_position;
    }
    dst_position.previous = &dst_position; // Can't assume stack here.
}
