#pragma once
#include <cstdint>

#include "bitboard.h"

/**
 * @brief Bitboard bit sets for a specific square.
*/
struct Sets
{
    Bitboard north;
    Bitboard northeast;
    Bitboard east;
    Bitboard southeast;
    Bitboard south;
    Bitboard southwest;
    Bitboard west;
    Bitboard northwest;
    union
    {
        Bitboard pawn_attacks[2];   /**< indexed by color */
        struct
        {
            Bitboard pawn_attacks_white;    /**< squares attacked by a white pawn on this square    */
            Bitboard pawn_attacks_black;    /**< squares attacked by a black pawn on this square    */
        };
    };
    Bitboard knight_attacks;    /**< squares attacked by a knight on this square    */
    Bitboard bishop_attacks;    /**< squares attacked by a bishop on an empty board */
    Bitboard rook_attacks;      /**< squares attacked by a rook on an empty board   */
    Bitboard queen_attacks;     /**< squares attacked by a queen on an empty board  */
    Bitboard king_attacks;      /**< squares attacked by a king on this square      */
    Bitboard king_attacks2;     /**< squares within 2 king moves from this square   */
    union
    {
        Bitboard king_pawn_shelter[2];
        struct 
        {
            Bitboard king_pawn_shelter_white;
            Bitboard king_pawn_shelter_black;
        };
    };
};

/**
 * @brief Bitsets used to efficiently determine pawn structure.
 * A passed pawn has no enemy pawns in front of it either on its file or on adjacent files.
 * 
 * An isolated pawn has no friendly pawns on either adjacent file.
 * 
 * A supported pawn has at least one friendly pawn on an adjacent file, either level
 * with it, or behind it. A pawn which is not supported may be a backwards pawn, if the
 * square in front of it is attacked by an enemy pawn.
 * 
 * A doubled pawn has a friendly pawn in front of it on the same file.
*/
struct PawnSets
{
    Bitboard passed_pawn_mask;      /**< Squares which if not containing an enemy pawn, make the pawn passed.       */
    Bitboard isolated_pawn_mask;    /**< Squares which if not containing a friendly pawn, make the pawn isolated.   */
    Bitboard supported_pawn_mask;   /**< Squares which if containing a friendly pawn, make the pawn supported.      */
    Bitboard doubled_pawn_mask;     /**< Squares which if containing a friendly pawn, make the pawn doubled.        */
};

/**
 * @brief An entry in the magic bitboard move generator array.
 * Each entry contains information to generate sliding moves
 * for either a bishop or a rook, for one square.
 * Uses an extra indirection via indices, since indices are 1 byte
 * each and attacks are 8 bytes each. This saves a lot of space
 * in the (already very large) rook magic bitboard tables, since for
 * exmaple for rooks there are 12 bits in the occupancy mask (4096 indices)
 * but typically only around 100 actual move sets.
 * 
 * To get sliding move targets without branches or loops we use:
 * 
 * m->attacks[m->indices[((occupied_squares & m->occupancy_mask) * m->magic) >> m->shift]]
 * 
 * The magic bitboard sets are generated at compile time by 
 * compiling and then running "generate_constants.cpp"
 */
struct MagicMoveEntry
{
    uint64_t        magic;          /**< Magic multiplier.                              */
    Bitboard        occupancy_mask; /**< Occupancy mask (excludes final target square). */
    int             shift;          /**< Number of bits to right shift to get indices.  */
    const uint64_t* attacks;        /**< Discrete attack vectors (move sets).           */
    const uint8_t*  indices;        /**< Indices into the discrete attack vector array. */
};

extern const Sets           SETS[64];
extern const PawnSets       PAWN_SETS[2][64];
extern const Bitboard       INTERVENING_SQUARES[64][64];
extern const uint64_t       CASTLING_RIGHTS_HASHES[16];
extern const uint64_t       EN_PASSANT_HASHES[64];
extern const uint64_t       PIECE_SQUARE_HASHES[2][6][64];
extern const MagicMoveEntry ROOK_MAGICS[64];
extern const MagicMoveEntry BISHOP_MAGICS[64];
