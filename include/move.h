#pragma once
/**
 * @file Macros for constructing and using chess moves.
 *
 * Moves are contained within a 64 bit integer.
 *
 *   BITS      INTERPRETATION
 *  0 -  5     to (destination square index)
 *  6 - 11     from (source square index)
 * 12 - 14     moving piece type
 * 15 - 17     captured piece type in the case of capture moves
 * 18 - 20     promoted piece type in the case of pawn promotions
 * 21 - 21     castling move flag
 * 22 - 22     en passant capture flag
 * 23 - 23     pawn double push flag
 * 32 - 63     may contain move score (as signed int) when move has been evaluated
 * 
 * A value of 0 terminates a move list.
*/
#define CONSTRUCT_PROMOTION_MOVE(from, to, captured, promoted)  ((to) | ((from) << 6) | (PAWN    << 12) | ((captured) << 15) | ((promoted) << 18))
#define CONSTRUCT_CASTLING_MOVE(from, to)                       ((to) | ((from) << 6) | (KING    << 12) |                      (1 << 21))
#define CONSTRUCT_EP_CAPTURE_MOVE(from, to)                     ((to) | ((from) << 6) | (PAWN    << 12) | (PAWN       << 15) | (1 << 22))
#define CONSTRUCT_PAWN_DOUBLE_PUSH_MOVE(from, to)               ((to) | ((from) << 6) | (PAWN    << 12) |                      (1 << 23))
#define CONSTRUCT_CAPTURE_MOVE(from, to, piece, captured)       ((to) | ((from) << 6) | ((piece) << 12) | ((captured) << 15))
#define CONSTRUCT_NON_CAPTURE_MOVE(from, to, piece)             ((to) | ((from) << 6) | ((piece) << 12))

#define MOVE_TO(m)                  ( (m)        & 0x3F)
#define MOVE_FROM(m)                (((m) >>  6) & 0x3F)
#define MOVE_PIECE(m)               (((m) >> 12) & 0x07)
#define MOVE_CAPTURED(m)            (((m) >> 15) & 0x07)
#define MOVE_PROMOTED(m)            (((m) >> 18) & 0x07)
#define MOVE_IS_CASTLING(m)         ( (m)        & (1 << 21))
#define MOVE_IS_EP_CAPTURE(m)       ( (m)        & (1 << 22))
#define MOVE_IS_PAWN_DOUBLE_PUSH(m) ( (m)        & (1 << 23))
#define MOVE_SCORE(m)               ((int)((m) >> 32))
#define MOVE_BITS(m)                ( (m) & 0xFFFFFFFF)
#define SCORED_MOVE(m,s)            (((m) & 0xFFFFFFFF) | ((int64_t)(s) << 32))