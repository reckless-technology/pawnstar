#pragma once
/**
 * @brief Various useful bitboard constants.
 */

/*
A bitboard is just a set-wise interpretation of a 64-bit unsigned integer, with 
each bit mapping to a square on the chessboard. 

If the bit is 1, then the corresponding square is a member of that set, for example:

The set of squares occupied by pawns
The set of squares occupied by a black piece
The set of squares to which a knight on e4 may move
The set of squares attacked by black queens

Bit  0 maps to square a1 (LSB)
Bit  7 maps to square h1
Bit 56 maps to square a8
Bit 63 maps to square h8 (MSB)

This is commonly referred to as LERF (little endian rank file mapping). 
If you treat the board as a 2D array, with square (0,0) being a1 and square 
(7,7) being h8, then the bitboard for a given square is just 

(1ull << (x + 8 * y))

Individual square bitboards
*/
#define A1BB            0x0000000000000001ull
#define B1BB            0x0000000000000002ull
#define C1BB            0x0000000000000004ull
#define D1BB            0x0000000000000008ull
#define E1BB            0x0000000000000010ull
#define F1BB            0x0000000000000020ull
#define G1BB            0x0000000000000040ull
#define H1BB            0x0000000000000080ull
#define A2BB            0x0000000000000100ull
#define B2BB            0x0000000000000200ull
#define C2BB            0x0000000000000400ull
#define D2BB            0x0000000000000800ull
#define E2BB            0x0000000000001000ull
#define F2BB            0x0000000000002000ull
#define G2BB            0x0000000000004000ull
#define H2BB            0x0000000000008000ull
#define A3BB            0x0000000000010000ull
#define B3BB            0x0000000000020000ull
#define C3BB            0x0000000000040000ull
#define D3BB            0x0000000000080000ull
#define E3BB            0x0000000000100000ull
#define F3BB            0x0000000000200000ull
#define G3BB            0x0000000000400000ull
#define H3BB            0x0000000000800000ull
#define A4BB            0x0000000001000000ull
#define B4BB            0x0000000002000000ull
#define C4BB            0x0000000004000000ull
#define D4BB            0x0000000008000000ull
#define E4BB            0x0000000010000000ull
#define F4BB            0x0000000020000000ull
#define G4BB            0x0000000040000000ull
#define H4BB            0x0000000080000000ull
#define A5BB            0x0000000100000000ull
#define B5BB            0x0000000200000000ull
#define C5BB            0x0000000400000000ull
#define D5BB            0x0000000800000000ull
#define E5BB            0x0000001000000000ull
#define F5BB            0x0000002000000000ull
#define G5BB            0x0000004000000000ull
#define H5BB            0x0000008000000000ull
#define A6BB            0x0000010000000000ull
#define B6BB            0x0000020000000000ull
#define C6BB            0x0000040000000000ull
#define D6BB            0x0000080000000000ull
#define E6BB            0x0000100000000000ull
#define F6BB            0x0000200000000000ull
#define G6BB            0x0000400000000000ull
#define H6BB            0x0000800000000000ull
#define A7BB            0x0001000000000000ull
#define B7BB            0x0002000000000000ull
#define C7BB            0x0004000000000000ull
#define D7BB            0x0008000000000000ull
#define E7BB            0x0010000000000000ull
#define F7BB            0x0020000000000000ull
#define G7BB            0x0040000000000000ull
#define H7BB            0x0080000000000000ull
#define A8BB            0x0100000000000000ull
#define B8BB            0x0200000000000000ull
#define C8BB            0x0400000000000000ull
#define D8BB            0x0800000000000000ull
#define E8BB            0x1000000000000000ull
#define F8BB            0x2000000000000000ull
#define G8BB            0x4000000000000000ull
#define H8BB            0x8000000000000000ull
/*
Rank bitboards
*/
#define RANK_1          0x00000000000000FFull
#define RANK_2          0x000000000000FF00ull
#define RANK_3          0x0000000000FF0000ull
#define RANK_4          0x00000000FF000000ull
#define RANK_5          0x000000FF00000000ull
#define RANK_6          0x0000FF0000000000ull
#define RANK_7          0x00FF000000000000ull
#define RANK_8          0xFF00000000000000ull
/*
File bitboards
*/
#define FILE_A          0x0101010101010101ull
#define FILE_B          0x0202020202020202ull
#define FILE_C          0x0404040404040404ull
#define FILE_D          0x0808080808080808ull
#define FILE_E          0x1010101010101010ull
#define FILE_F          0x2020202020202020ull
#define FILE_G          0x4040404040404040ull
#define FILE_H          0x8080808080808080ull
/*
Bitboards to mask off edge files to prevent wraparound 
when shifting horizontally or diagonally
*/
#define MASK_EAST_1     0x7F7F7F7F7F7F7F7Full   
#define MASK_EAST_2     0x3F3F3F3F3F3F3F3Full   
#define MASK_EAST_4     0x0F0F0F0F0F0F0F0Full   
#define MASK_WEST_1     0xFEFEFEFEFEFEFEFEull   
#define MASK_WEST_2     0xFCFCFCFCFCFCFCFCull   
#define MASK_WEST_4     0xF0F0F0F0F0F0F0F0ull   
/*
Bitboards of useful square sets
*/
#define ALL_SQUARES     0xFFFFFFFFFFFFFFFFull
#define NO_SQUARES      0x0000000000000000ull
#define WHITE_SQUARES   0x55AA55AA55AA55AAull
#define BLACK_SQUARES   0xAA55AA55AA55AA55ull
#define CORNER_SQUARES  0x8100000000000081ull
#define BORDER_SQUARES  0xFF818181818181FFull
#define CTR_16_SQUARES  0x00003C3C3C3C0000ull
#define CTR_4_SQUARES   0x0000001818000000ull
/*
Hash value for black to move - nothing special just a big random number
*/
#define BLACK_MOVE_HASH 0xB92B78FCCF92F8CDull