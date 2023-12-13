#include "pawnstar.h"

/**
 * @brief Convert a move to standard algebraic notation string.
 * @param position Current position corresponding to the move.
 * @param the_move The move input to be formatted as a string.
 * @param move_string Where to store the move string.
 * @return Number of characters written to output.
 */
int MoveToString(const Position& position, Move move, char move_string[])
{
    const char* initial_ptr = move_string;
    Move legal_moves[MAX_MOVES_PER_POSITION];
    char disambiguation[3]   = { 0 };
    char from_square[3]      = { FileChar(MoveFrom(move)), RankChar(MoveFrom(move)), 0 };
    char to_square[3]        = { FileChar(MoveTo  (move)), RankChar(MoveTo  (move)), 0 };
    bool is_source_ambiguous = false;
    bool is_file_unique      = true;
    bool is_rank_unique      = true;   
    const char move_piece    = " PNBRQK"[MovePiece(move)];
    /*
    Determine if there is more than one piece of the same type which is capable 
    of moving to the target square, and will require further disambiguation
    */
    GenerateLegalMoves(position, legal_moves);
    for (const Move* m = legal_moves; *m; ++m)
    {
        if (MovePiece(*m) == MovePiece(move) && 
            MoveTo   (*m) == MoveTo   (move) &&
            MoveFrom (*m) != MoveFrom (move))
        {
            is_source_ambiguous = true;
            if (FileOf(MoveFrom(*m)) == FileOf(MoveFrom(move)))
            {
                is_file_unique = false;
            }
            if (RankOf(MoveFrom(*m)) == RankOf(MoveFrom(move)))
            {
                is_rank_unique = false;
            }   
        }
    }
    /*
    Determine how to disambiguate source square based on the uniqueness of the 
    source rank and file
    */
    if (is_source_ambiguous)
    {
        if (is_file_unique)
        {
            disambiguation[0] = from_square[0]; 
        }
        else if (is_rank_unique)
        {
            disambiguation[0] = from_square[1];
        }
        else
        {
            disambiguation[0] = from_square[0];
            disambiguation[1] = from_square[1];
        }
    }
    switch (MovePiece(move))
    {
    RegularMove:
    default:
    case KNIGHT:
    case BISHOP:
    case ROOK:
    case QUEEN:
        if (MoveCaptured(move))
        {
            move_string += sprintf(move_string, "%c%sx%s", move_piece, disambiguation, to_square);
        }
        else
        {
            move_string += sprintf(move_string, "%c%s%s", move_piece, disambiguation, to_square);
        }
        break;
    
    case KING:
        if (IsCastlingMove(move))
        {
            switch (MoveTo(move))
            {
            case G1:
            case G8:
                move_string += sprintf(move_string, "O-O");
                break;
            case C1:
            case C8:
                move_string += sprintf(move_string, "O-O-O");
                break;
            }
        }
        else
        {
            goto RegularMove;
        }
        break;

    case PAWN:
        if (MoveCaptured(move))
        {
            move_string += sprintf(move_string, "%cx%s", from_square[0], to_square);
            if (IsEpCaptureMove(move))
            {
                /* ep capture */
                move_string += sprintf(move_string, "e.p.");
            }
        }
        else
        {
            move_string += sprintf(move_string, "%s", to_square); 
        }
        if (MovePromoted(move))
        {
            move_string += sprintf(move_string, "=%c", "  NBRQ"[MovePromoted(move)]);
        }
        break;
    }    
    /*
    Determine if this move results in check or checkmate
    */
    Position dst_position { position, move };
    if (dst_position.IsCheckmate())
    {
        move_string += sprintf(move_string, "#");
    }
    else if (dst_position.flags_ & IS_CHECK)
    {
        move_string += sprintf(move_string, "+");
    }
    return (int)(move_string - initial_ptr);
}

/**
 * @brief Convert a sequence of moves into a string in SAN format.
 * @param position Starting position.
 * @param moves Zero terminated list of moves.
 * @param move_string Pointer to store the formatted string.
 * @return Number of characters written to output.
 */
int MoveSequenceToSanString(const Position& position, const Move* moves, char* move_string)
{
    const char* const initial_ptr = move_string;
    bool is_first_move = true;
    Position src_position { position };
    for (const Move* move = moves; *move; ++move)
    {
        if (!is_first_move)
        {
            *move_string++ = ' ';
        }
        move_string += MoveToString(src_position, *move, move_string);
        Position dst_position { src_position, *move };
        src_position = dst_position;
        is_first_move = false;
    }
    return (int)(move_string - initial_ptr);
}