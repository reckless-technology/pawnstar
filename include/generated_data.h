#pragma once
#include "bitboard.h"
#include <cstdint>

/// @file Declares data which is generated at compile time by the program "generate_constants.cpp" and contains various
/// precomputed constants that are used by pawnstar to speed up positional hashing, move generation, attack detection
/// and pawn structure analysis.

/// @brief Bitboard sets pertaining to a single square on the board.
struct Sets
{
    Bitboard north;                   ///< all squares north of here
    Bitboard northeast;               ///< all squares northeast of here
    Bitboard east;                    ///< all squares east of here
    Bitboard southeast;               ///< all squares southeast of here
    Bitboard south;                   ///< all squares south of here
    Bitboard southwest;               ///< all squares southwest of here
    Bitboard west;                    ///< all squares west of here
    Bitboard northwest;               ///< all squares northwest of here
    Bitboard pawn_attacks_white;      ///< squares attacked by a white pawn on this square
    Bitboard pawn_attacks_black;      ///< squares attacked by a black pawn on this square
    Bitboard knight_attacks;          ///< squares attacked by a knight on this square
    Bitboard bishop_attacks;          ///< squares attacked by a bishop on an empty board
    Bitboard rook_attacks;            ///< squares attacked by a rook on an empty board
    Bitboard queen_attacks;           ///< squares attacked by a queen on an empty board
    Bitboard king_attacks;            ///< squares attacked by a king on this square
    Bitboard king_attacks2;           ///< squares within 2 king moves from this square
    Bitboard king_pawn_shelter_white; ///< pawns in front of the white king
    Bitboard king_pawn_shelter_black; ///< pawns in front of the black king

    /// @brief Pawn attacks for a single color.
    /// @param color The color.
    /// @return Squares attacked by a pawn of that color.
    Bitboard PawnAttacks(Color color) const
    {
        return (&pawn_attacks_white)[color];
    }

    /// @brief King pawn shelter for a single color.
    /// @param color The color.
    /// @return Squares in front of the king which comprise the king's pawn shelter.
    Bitboard KingPawnShelter(Color color) const
    {
        return (&king_pawn_shelter_white)[color];
    }
};

///@brief Bitsets used to efficiently determine pawn structure.
/// A passed pawn has no enemy pawns in front of it either on its file or on adjacent files.
/// An isolated pawn has no friendly pawns on either adjacent file.
/// A supported pawn has at least one friendly pawn on an adjacent file, either level with it, or behind it. A pawn
/// which is not supported may be a backwards pawn, if the square in front of it is also attacked by an enemy pawn.
/// A doubled pawn has a friendly pawn in front of it on the same file.
struct PawnSets
{
    Bitboard passed_pawn_mask;    ///< Squares which if not containing an enemy pawn, make the pawn passed.
    Bitboard isolated_pawn_mask;  ///< Squares which if not containing a friendly pawn, make the pawn isolated.
    Bitboard supported_pawn_mask; ///< Squares which if containing a friendly pawn, make the pawn supported.
    Bitboard doubled_pawn_mask;   ///< Squares which if containing a friendly pawn, make the pawn doubled.
};

///@brief An entry in the magic bitboard move generator array.
/// Each entry contains information to generate sliding moves for either a bishop or a rook, for one square.
/// Uses an extra indirection via indices, since indices are 1 byte each and attack sets are 8 bytes each. This saves a
/// lot of space in the (already very large) rook magic bitboard tables, since for examaple for rooks there are 12 bits
/// in the occupancy mask (4096 indices) but typically only around 100 actual move sets. To get the sliding piece move
/// targets without branches or loops we use:
///
/// m.attacks[m.indices[((occupied_squares & m.occupancy_mask) * m.magic) >> m.shift]]
///
/// The magic bitboard sets are generated at compile time by compiling and then running "generate_constants.cpp"
struct MagicMoveEntry
{
    uint64_t        magic;          ///< Magic multiplier.
    Bitboard        occupancy_mask; ///< Occupancy mask (excludes final target square).
    int             shift;          ///< Number of bits to right shift to get indices.
    const uint64_t *attacks;        ///< Discrete attack vectors (move sets).
    const uint8_t  *indices;        ///< Indices into the discrete attack vector array.
};

extern const Sets           SETS[64];                      ///< Sets for each square on the board.
extern const PawnSets       PAWN_SETS[2][64];              ///< Sets for each square for white and black.
extern const Bitboard       INTERVENING_SQUARES[64][64];   ///< Squares between 2 squares (if they are colinear).
extern const uint64_t       CASTLING_RIGHTS_HASHES[16];    ///< Zobrist hashes for castling rights.
extern const uint64_t       EN_PASSANT_HASHES[64];         ///< Zobrist hashes for en passant capture avilability.
extern const uint64_t       PIECE_SQUARE_HASHES[2][6][64]; ///< Zobrist hashes for pieces on the board.
extern const MagicMoveEntry ROOK_MAGICS[64];               ///< Magic move entries for rook moves.
extern const MagicMoveEntry BISHOP_MAGICS[64];             ///< Magic move entries for bishop moves.
