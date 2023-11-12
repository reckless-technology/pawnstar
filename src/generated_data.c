/* This file was generated on Nov 12 2023 at 08:35:13 */
#include "types.h"
const Sets SETS[64] = 
{
    { /* square a1 */
        .north                = 0x0101010101010100ull, /* popcnt  7 */
        .northeast            = 0x8040201008040200ull, /* popcnt  7 */
        .east                 = 0x00000000000000FEull, /* popcnt  7 */
        .southeast            = 0x0000000000000000ull, /* popcnt  0 */
        .south                = 0x0000000000000000ull, /* popcnt  0 */
        .southwest            = 0x0000000000000000ull, /* popcnt  0 */
        .west                 = 0x0000000000000000ull, /* popcnt  0 */
        .northwest            = 0x0000000000000000ull, /* popcnt  0 */
        .pawn_attacks_white   = 0x0000000000000200ull, /* popcnt  1 */
        .pawn_attacks_black   = 0x0000000000000000ull, /* popcnt  0 */
        .knight_attacks       = 0x0000000000020400ull, /* popcnt  2 */
        .bishop_attacks       = 0x8040201008040200ull, /* popcnt  7 */
        .rook_attacks         = 0x01010101010101FEull, /* popcnt 14 */
        .queen_attacks        = 0x81412111090503FEull, /* popcnt 21 */
        .king_attacks         = 0x0000000000000302ull, /* popcnt  3 */
    },
    { /* square b1 */
        .north                = 0x0202020202020200ull, /* popcnt  7 */
        .northeast            = 0x0080402010080400ull, /* popcnt  6 */
        .east                 = 0x00000000000000FCull, /* popcnt  6 */
        .southeast            = 0x0000000000000000ull, /* popcnt  0 */
        .south                = 0x0000000000000000ull, /* popcnt  0 */
        .southwest            = 0x0000000000000000ull, /* popcnt  0 */
        .west                 = 0x0000000000000001ull, /* popcnt  1 */
        .northwest            = 0x0000000000000100ull, /* popcnt  1 */
        .pawn_attacks_white   = 0x0000000000000500ull, /* popcnt  2 */
        .pawn_attacks_black   = 0x0000000000000000ull, /* popcnt  0 */
        .knight_attacks       = 0x0000000000050800ull, /* popcnt  3 */
        .bishop_attacks       = 0x0080402010080500ull, /* popcnt  7 */
        .rook_attacks         = 0x02020202020202FDull, /* popcnt 14 */
        .queen_attacks        = 0x02824222120A07FDull, /* popcnt 21 */
        .king_attacks         = 0x0000000000000705ull, /* popcnt  5 */
    },
    { /* square c1 */
        .north                = 0x0404040404040400ull, /* popcnt  7 */
        .northeast            = 0x0000804020100800ull, /* popcnt  5 */
        .east                 = 0x00000000000000F8ull, /* popcnt  5 */
        .southeast            = 0x0000000000000000ull, /* popcnt  0 */
        .south                = 0x0000000000000000ull, /* popcnt  0 */
        .southwest            = 0x0000000000000000ull, /* popcnt  0 */
        .west                 = 0x0000000000000003ull, /* popcnt  2 */
        .northwest            = 0x0000000000010200ull, /* popcnt  2 */
        .pawn_attacks_white   = 0x0000000000000A00ull, /* popcnt  2 */
        .pawn_attacks_black   = 0x0000000000000000ull, /* popcnt  0 */
        .knight_attacks       = 0x00000000000A1100ull, /* popcnt  4 */
        .bishop_attacks       = 0x0000804020110A00ull, /* popcnt  7 */
        .rook_attacks         = 0x04040404040404FBull, /* popcnt 14 */
        .queen_attacks        = 0x0404844424150EFBull, /* popcnt 21 */
        .king_attacks         = 0x0000000000000E0Aull, /* popcnt  5 */
    },
    { /* square d1 */
        .north                = 0x0808080808080800ull, /* popcnt  7 */
        .northeast            = 0x0000008040201000ull, /* popcnt  4 */
        .east                 = 0x00000000000000F0ull, /* popcnt  4 */
        .southeast            = 0x0000000000000000ull, /* popcnt  0 */
        .south                = 0x0000000000000000ull, /* popcnt  0 */
        .southwest            = 0x0000000000000000ull, /* popcnt  0 */
        .west                 = 0x0000000000000007ull, /* popcnt  3 */
        .northwest            = 0x0000000001020400ull, /* popcnt  3 */
        .pawn_attacks_white   = 0x0000000000001400ull, /* popcnt  2 */
        .pawn_attacks_black   = 0x0000000000000000ull, /* popcnt  0 */
        .knight_attacks       = 0x0000000000142200ull, /* popcnt  4 */
        .bishop_attacks       = 0x0000008041221400ull, /* popcnt  7 */
        .rook_attacks         = 0x08080808080808F7ull, /* popcnt 14 */
        .queen_attacks        = 0x08080888492A1CF7ull, /* popcnt 21 */
        .king_attacks         = 0x0000000000001C14ull, /* popcnt  5 */
    },
    { /* square e1 */
        .north                = 0x1010101010101000ull, /* popcnt  7 */
        .northeast            = 0x0000000080402000ull, /* popcnt  3 */
        .east                 = 0x00000000000000E0ull, /* popcnt  3 */
        .southeast            = 0x0000000000000000ull, /* popcnt  0 */
        .south                = 0x0000000000000000ull, /* popcnt  0 */
        .southwest            = 0x0000000000000000ull, /* popcnt  0 */
        .west                 = 0x000000000000000Full, /* popcnt  4 */
        .northwest            = 0x0000000102040800ull, /* popcnt  4 */
        .pawn_attacks_white   = 0x0000000000002800ull, /* popcnt  2 */
        .pawn_attacks_black   = 0x0000000000000000ull, /* popcnt  0 */
        .knight_attacks       = 0x0000000000284400ull, /* popcnt  4 */
        .bishop_attacks       = 0x0000000182442800ull, /* popcnt  7 */
        .rook_attacks         = 0x10101010101010EFull, /* popcnt 14 */
        .queen_attacks        = 0x10101011925438EFull, /* popcnt 21 */
        .king_attacks         = 0x0000000000003828ull, /* popcnt  5 */
    },
    { /* square f1 */
        .north                = 0x2020202020202000ull, /* popcnt  7 */
        .northeast            = 0x0000000000804000ull, /* popcnt  2 */
        .east                 = 0x00000000000000C0ull, /* popcnt  2 */
        .southeast            = 0x0000000000000000ull, /* popcnt  0 */
        .south                = 0x0000000000000000ull, /* popcnt  0 */
        .southwest            = 0x0000000000000000ull, /* popcnt  0 */
        .west                 = 0x000000000000001Full, /* popcnt  5 */
        .northwest            = 0x0000010204081000ull, /* popcnt  5 */
        .pawn_attacks_white   = 0x0000000000005000ull, /* popcnt  2 */
        .pawn_attacks_black   = 0x0000000000000000ull, /* popcnt  0 */
        .knight_attacks       = 0x0000000000508800ull, /* popcnt  4 */
        .bishop_attacks       = 0x0000010204885000ull, /* popcnt  7 */
        .rook_attacks         = 0x20202020202020DFull, /* popcnt 14 */
        .queen_attacks        = 0x2020212224A870DFull, /* popcnt 21 */
        .king_attacks         = 0x0000000000007050ull, /* popcnt  5 */
    },
    { /* square g1 */
        .north                = 0x4040404040404000ull, /* popcnt  7 */
        .northeast            = 0x0000000000008000ull, /* popcnt  1 */
        .east                 = 0x0000000000000080ull, /* popcnt  1 */
        .southeast            = 0x0000000000000000ull, /* popcnt  0 */
        .south                = 0x0000000000000000ull, /* popcnt  0 */
        .southwest            = 0x0000000000000000ull, /* popcnt  0 */
        .west                 = 0x000000000000003Full, /* popcnt  6 */
        .northwest            = 0x0001020408102000ull, /* popcnt  6 */
        .pawn_attacks_white   = 0x000000000000A000ull, /* popcnt  2 */
        .pawn_attacks_black   = 0x0000000000000000ull, /* popcnt  0 */
        .knight_attacks       = 0x0000000000A01000ull, /* popcnt  3 */
        .bishop_attacks       = 0x000102040810A000ull, /* popcnt  7 */
        .rook_attacks         = 0x40404040404040BFull, /* popcnt 14 */
        .queen_attacks        = 0x404142444850E0BFull, /* popcnt 21 */
        .king_attacks         = 0x000000000000E0A0ull, /* popcnt  5 */
    },
    { /* square h1 */
        .north                = 0x8080808080808000ull, /* popcnt  7 */
        .northeast            = 0x0000000000000000ull, /* popcnt  0 */
        .east                 = 0x0000000000000000ull, /* popcnt  0 */
        .southeast            = 0x0000000000000000ull, /* popcnt  0 */
        .south                = 0x0000000000000000ull, /* popcnt  0 */
        .southwest            = 0x0000000000000000ull, /* popcnt  0 */
        .west                 = 0x000000000000007Full, /* popcnt  7 */
        .northwest            = 0x0102040810204000ull, /* popcnt  7 */
        .pawn_attacks_white   = 0x0000000000004000ull, /* popcnt  1 */
        .pawn_attacks_black   = 0x0000000000000000ull, /* popcnt  0 */
        .knight_attacks       = 0x0000000000402000ull, /* popcnt  2 */
        .bishop_attacks       = 0x0102040810204000ull, /* popcnt  7 */
        .rook_attacks         = 0x808080808080807Full, /* popcnt 14 */
        .queen_attacks        = 0x8182848890A0C07Full, /* popcnt 21 */
        .king_attacks         = 0x000000000000C040ull, /* popcnt  3 */
    },
    { /* square a2 */
        .north                = 0x0101010101010000ull, /* popcnt  6 */
        .northeast            = 0x4020100804020000ull, /* popcnt  6 */
        .east                 = 0x000000000000FE00ull, /* popcnt  7 */
        .southeast            = 0x0000000000000002ull, /* popcnt  1 */
        .south                = 0x0000000000000001ull, /* popcnt  1 */
        .southwest            = 0x0000000000000000ull, /* popcnt  0 */
        .west                 = 0x0000000000000000ull, /* popcnt  0 */
        .northwest            = 0x0000000000000000ull, /* popcnt  0 */
        .pawn_attacks_white   = 0x0000000000020000ull, /* popcnt  1 */
        .pawn_attacks_black   = 0x0000000000000002ull, /* popcnt  1 */
        .knight_attacks       = 0x0000000002040004ull, /* popcnt  3 */
        .bishop_attacks       = 0x4020100804020002ull, /* popcnt  7 */
        .rook_attacks         = 0x010101010101FE01ull, /* popcnt 14 */
        .queen_attacks        = 0x412111090503FE03ull, /* popcnt 21 */
        .king_attacks         = 0x0000000000030203ull, /* popcnt  5 */
    },
    { /* square b2 */
        .north                = 0x0202020202020000ull, /* popcnt  6 */
        .northeast            = 0x8040201008040000ull, /* popcnt  6 */
        .east                 = 0x000000000000FC00ull, /* popcnt  6 */
        .southeast            = 0x0000000000000004ull, /* popcnt  1 */
        .south                = 0x0000000000000002ull, /* popcnt  1 */
        .southwest            = 0x0000000000000001ull, /* popcnt  1 */
        .west                 = 0x0000000000000100ull, /* popcnt  1 */
        .northwest            = 0x0000000000010000ull, /* popcnt  1 */
        .pawn_attacks_white   = 0x0000000000050000ull, /* popcnt  2 */
        .pawn_attacks_black   = 0x0000000000000005ull, /* popcnt  2 */
        .knight_attacks       = 0x0000000005080008ull, /* popcnt  4 */
        .bishop_attacks       = 0x8040201008050005ull, /* popcnt  9 */
        .rook_attacks         = 0x020202020202FD02ull, /* popcnt 14 */
        .queen_attacks        = 0x824222120A07FD07ull, /* popcnt 23 */
        .king_attacks         = 0x0000000000070507ull, /* popcnt  8 */
    },
    { /* square c2 */
        .north                = 0x0404040404040000ull, /* popcnt  6 */
        .northeast            = 0x0080402010080000ull, /* popcnt  5 */
        .east                 = 0x000000000000F800ull, /* popcnt  5 */
        .southeast            = 0x0000000000000008ull, /* popcnt  1 */
        .south                = 0x0000000000000004ull, /* popcnt  1 */
        .southwest            = 0x0000000000000002ull, /* popcnt  1 */
        .west                 = 0x0000000000000300ull, /* popcnt  2 */
        .northwest            = 0x0000000001020000ull, /* popcnt  2 */
        .pawn_attacks_white   = 0x00000000000A0000ull, /* popcnt  2 */
        .pawn_attacks_black   = 0x000000000000000Aull, /* popcnt  2 */
        .knight_attacks       = 0x000000000A110011ull, /* popcnt  6 */
        .bishop_attacks       = 0x00804020110A000Aull, /* popcnt  9 */
        .rook_attacks         = 0x040404040404FB04ull, /* popcnt 14 */
        .queen_attacks        = 0x04844424150EFB0Eull, /* popcnt 23 */
        .king_attacks         = 0x00000000000E0A0Eull, /* popcnt  8 */
    },
    { /* square d2 */
        .north                = 0x0808080808080000ull, /* popcnt  6 */
        .northeast            = 0x0000804020100000ull, /* popcnt  4 */
        .east                 = 0x000000000000F000ull, /* popcnt  4 */
        .southeast            = 0x0000000000000010ull, /* popcnt  1 */
        .south                = 0x0000000000000008ull, /* popcnt  1 */
        .southwest            = 0x0000000000000004ull, /* popcnt  1 */
        .west                 = 0x0000000000000700ull, /* popcnt  3 */
        .northwest            = 0x0000000102040000ull, /* popcnt  3 */
        .pawn_attacks_white   = 0x0000000000140000ull, /* popcnt  2 */
        .pawn_attacks_black   = 0x0000000000000014ull, /* popcnt  2 */
        .knight_attacks       = 0x0000000014220022ull, /* popcnt  6 */
        .bishop_attacks       = 0x0000804122140014ull, /* popcnt  9 */
        .rook_attacks         = 0x080808080808F708ull, /* popcnt 14 */
        .queen_attacks        = 0x080888492A1CF71Cull, /* popcnt 23 */
        .king_attacks         = 0x00000000001C141Cull, /* popcnt  8 */
    },
    { /* square e2 */
        .north                = 0x1010101010100000ull, /* popcnt  6 */
        .northeast            = 0x0000008040200000ull, /* popcnt  3 */
        .east                 = 0x000000000000E000ull, /* popcnt  3 */
        .southeast            = 0x0000000000000020ull, /* popcnt  1 */
        .south                = 0x0000000000000010ull, /* popcnt  1 */
        .southwest            = 0x0000000000000008ull, /* popcnt  1 */
        .west                 = 0x0000000000000F00ull, /* popcnt  4 */
        .northwest            = 0x0000010204080000ull, /* popcnt  4 */
        .pawn_attacks_white   = 0x0000000000280000ull, /* popcnt  2 */
        .pawn_attacks_black   = 0x0000000000000028ull, /* popcnt  2 */
        .knight_attacks       = 0x0000000028440044ull, /* popcnt  6 */
        .bishop_attacks       = 0x0000018244280028ull, /* popcnt  9 */
        .rook_attacks         = 0x101010101010EF10ull, /* popcnt 14 */
        .queen_attacks        = 0x101011925438EF38ull, /* popcnt 23 */
        .king_attacks         = 0x0000000000382838ull, /* popcnt  8 */
    },
    { /* square f2 */
        .north                = 0x2020202020200000ull, /* popcnt  6 */
        .northeast            = 0x0000000080400000ull, /* popcnt  2 */
        .east                 = 0x000000000000C000ull, /* popcnt  2 */
        .southeast            = 0x0000000000000040ull, /* popcnt  1 */
        .south                = 0x0000000000000020ull, /* popcnt  1 */
        .southwest            = 0x0000000000000010ull, /* popcnt  1 */
        .west                 = 0x0000000000001F00ull, /* popcnt  5 */
        .northwest            = 0x0001020408100000ull, /* popcnt  5 */
        .pawn_attacks_white   = 0x0000000000500000ull, /* popcnt  2 */
        .pawn_attacks_black   = 0x0000000000000050ull, /* popcnt  2 */
        .knight_attacks       = 0x0000000050880088ull, /* popcnt  6 */
        .bishop_attacks       = 0x0001020488500050ull, /* popcnt  9 */
        .rook_attacks         = 0x202020202020DF20ull, /* popcnt 14 */
        .queen_attacks        = 0x20212224A870DF70ull, /* popcnt 23 */
        .king_attacks         = 0x0000000000705070ull, /* popcnt  8 */
    },
    { /* square g2 */
        .north                = 0x4040404040400000ull, /* popcnt  6 */
        .northeast            = 0x0000000000800000ull, /* popcnt  1 */
        .east                 = 0x0000000000008000ull, /* popcnt  1 */
        .southeast            = 0x0000000000000080ull, /* popcnt  1 */
        .south                = 0x0000000000000040ull, /* popcnt  1 */
        .southwest            = 0x0000000000000020ull, /* popcnt  1 */
        .west                 = 0x0000000000003F00ull, /* popcnt  6 */
        .northwest            = 0x0102040810200000ull, /* popcnt  6 */
        .pawn_attacks_white   = 0x0000000000A00000ull, /* popcnt  2 */
        .pawn_attacks_black   = 0x00000000000000A0ull, /* popcnt  2 */
        .knight_attacks       = 0x00000000A0100010ull, /* popcnt  4 */
        .bishop_attacks       = 0x0102040810A000A0ull, /* popcnt  9 */
        .rook_attacks         = 0x404040404040BF40ull, /* popcnt 14 */
        .queen_attacks        = 0x4142444850E0BFE0ull, /* popcnt 23 */
        .king_attacks         = 0x0000000000E0A0E0ull, /* popcnt  8 */
    },
    { /* square h2 */
        .north                = 0x8080808080800000ull, /* popcnt  6 */
        .northeast            = 0x0000000000000000ull, /* popcnt  0 */
        .east                 = 0x0000000000000000ull, /* popcnt  0 */
        .southeast            = 0x0000000000000000ull, /* popcnt  0 */
        .south                = 0x0000000000000080ull, /* popcnt  1 */
        .southwest            = 0x0000000000000040ull, /* popcnt  1 */
        .west                 = 0x0000000000007F00ull, /* popcnt  7 */
        .northwest            = 0x0204081020400000ull, /* popcnt  6 */
        .pawn_attacks_white   = 0x0000000000400000ull, /* popcnt  1 */
        .pawn_attacks_black   = 0x0000000000000040ull, /* popcnt  1 */
        .knight_attacks       = 0x0000000040200020ull, /* popcnt  3 */
        .bishop_attacks       = 0x0204081020400040ull, /* popcnt  7 */
        .rook_attacks         = 0x8080808080807F80ull, /* popcnt 14 */
        .queen_attacks        = 0x82848890A0C07FC0ull, /* popcnt 21 */
        .king_attacks         = 0x0000000000C040C0ull, /* popcnt  5 */
    },
    { /* square a3 */
        .north                = 0x0101010101000000ull, /* popcnt  5 */
        .northeast            = 0x2010080402000000ull, /* popcnt  5 */
        .east                 = 0x0000000000FE0000ull, /* popcnt  7 */
        .southeast            = 0x0000000000000204ull, /* popcnt  2 */
        .south                = 0x0000000000000101ull, /* popcnt  2 */
        .southwest            = 0x0000000000000000ull, /* popcnt  0 */
        .west                 = 0x0000000000000000ull, /* popcnt  0 */
        .northwest            = 0x0000000000000000ull, /* popcnt  0 */
        .pawn_attacks_white   = 0x0000000002000000ull, /* popcnt  1 */
        .pawn_attacks_black   = 0x0000000000000200ull, /* popcnt  1 */
        .knight_attacks       = 0x0000000204000402ull, /* popcnt  4 */
        .bishop_attacks       = 0x2010080402000204ull, /* popcnt  7 */
        .rook_attacks         = 0x0101010101FE0101ull, /* popcnt 14 */
        .queen_attacks        = 0x2111090503FE0305ull, /* popcnt 21 */
        .king_attacks         = 0x0000000003020300ull, /* popcnt  5 */
    },
    { /* square b3 */
        .north                = 0x0202020202000000ull, /* popcnt  5 */
        .northeast            = 0x4020100804000000ull, /* popcnt  5 */
        .east                 = 0x0000000000FC0000ull, /* popcnt  6 */
        .southeast            = 0x0000000000000408ull, /* popcnt  2 */
        .south                = 0x0000000000000202ull, /* popcnt  2 */
        .southwest            = 0x0000000000000100ull, /* popcnt  1 */
        .west                 = 0x0000000000010000ull, /* popcnt  1 */
        .northwest            = 0x0000000001000000ull, /* popcnt  1 */
        .pawn_attacks_white   = 0x0000000005000000ull, /* popcnt  2 */
        .pawn_attacks_black   = 0x0000000000000500ull, /* popcnt  2 */
        .knight_attacks       = 0x0000000508000805ull, /* popcnt  6 */
        .bishop_attacks       = 0x4020100805000508ull, /* popcnt  9 */
        .rook_attacks         = 0x0202020202FD0202ull, /* popcnt 14 */
        .queen_attacks        = 0x4222120A07FD070Aull, /* popcnt 23 */
        .king_attacks         = 0x0000000007050700ull, /* popcnt  8 */
    },
    { /* square c3 */
        .north                = 0x0404040404000000ull, /* popcnt  5 */
        .northeast            = 0x8040201008000000ull, /* popcnt  5 */
        .east                 = 0x0000000000F80000ull, /* popcnt  5 */
        .southeast            = 0x0000000000000810ull, /* popcnt  2 */
        .south                = 0x0000000000000404ull, /* popcnt  2 */
        .southwest            = 0x0000000000000201ull, /* popcnt  2 */
        .west                 = 0x0000000000030000ull, /* popcnt  2 */
        .northwest            = 0x0000000102000000ull, /* popcnt  2 */
        .pawn_attacks_white   = 0x000000000A000000ull, /* popcnt  2 */
        .pawn_attacks_black   = 0x0000000000000A00ull, /* popcnt  2 */
        .knight_attacks       = 0x0000000A1100110Aull, /* popcnt  8 */
        .bishop_attacks       = 0x804020110A000A11ull, /* popcnt 11 */
        .rook_attacks         = 0x0404040404FB0404ull, /* popcnt 14 */
        .queen_attacks        = 0x844424150EFB0E15ull, /* popcnt 25 */
        .king_attacks         = 0x000000000E0A0E00ull, /* popcnt  8 */
    },
    { /* square d3 */
        .north                = 0x0808080808000000ull, /* popcnt  5 */
        .northeast            = 0x0080402010000000ull, /* popcnt  4 */
        .east                 = 0x0000000000F00000ull, /* popcnt  4 */
        .southeast            = 0x0000000000001020ull, /* popcnt  2 */
        .south                = 0x0000000000000808ull, /* popcnt  2 */
        .southwest            = 0x0000000000000402ull, /* popcnt  2 */
        .west                 = 0x0000000000070000ull, /* popcnt  3 */
        .northwest            = 0x0000010204000000ull, /* popcnt  3 */
        .pawn_attacks_white   = 0x0000000014000000ull, /* popcnt  2 */
        .pawn_attacks_black   = 0x0000000000001400ull, /* popcnt  2 */
        .knight_attacks       = 0x0000001422002214ull, /* popcnt  8 */
        .bishop_attacks       = 0x0080412214001422ull, /* popcnt 11 */
        .rook_attacks         = 0x0808080808F70808ull, /* popcnt 14 */
        .queen_attacks        = 0x0888492A1CF71C2Aull, /* popcnt 25 */
        .king_attacks         = 0x000000001C141C00ull, /* popcnt  8 */
    },
    { /* square e3 */
        .north                = 0x1010101010000000ull, /* popcnt  5 */
        .northeast            = 0x0000804020000000ull, /* popcnt  3 */
        .east                 = 0x0000000000E00000ull, /* popcnt  3 */
        .southeast            = 0x0000000000002040ull, /* popcnt  2 */
        .south                = 0x0000000000001010ull, /* popcnt  2 */
        .southwest            = 0x0000000000000804ull, /* popcnt  2 */
        .west                 = 0x00000000000F0000ull, /* popcnt  4 */
        .northwest            = 0x0001020408000000ull, /* popcnt  4 */
        .pawn_attacks_white   = 0x0000000028000000ull, /* popcnt  2 */
        .pawn_attacks_black   = 0x0000000000002800ull, /* popcnt  2 */
        .knight_attacks       = 0x0000002844004428ull, /* popcnt  8 */
        .bishop_attacks       = 0x0001824428002844ull, /* popcnt 11 */
        .rook_attacks         = 0x1010101010EF1010ull, /* popcnt 14 */
        .queen_attacks        = 0x1011925438EF3854ull, /* popcnt 25 */
        .king_attacks         = 0x0000000038283800ull, /* popcnt  8 */
    },
    { /* square f3 */
        .north                = 0x2020202020000000ull, /* popcnt  5 */
        .northeast            = 0x0000008040000000ull, /* popcnt  2 */
        .east                 = 0x0000000000C00000ull, /* popcnt  2 */
        .southeast            = 0x0000000000004080ull, /* popcnt  2 */
        .south                = 0x0000000000002020ull, /* popcnt  2 */
        .southwest            = 0x0000000000001008ull, /* popcnt  2 */
        .west                 = 0x00000000001F0000ull, /* popcnt  5 */
        .northwest            = 0x0102040810000000ull, /* popcnt  5 */
        .pawn_attacks_white   = 0x0000000050000000ull, /* popcnt  2 */
        .pawn_attacks_black   = 0x0000000000005000ull, /* popcnt  2 */
        .knight_attacks       = 0x0000005088008850ull, /* popcnt  8 */
        .bishop_attacks       = 0x0102048850005088ull, /* popcnt 11 */
        .rook_attacks         = 0x2020202020DF2020ull, /* popcnt 14 */
        .queen_attacks        = 0x212224A870DF70A8ull, /* popcnt 25 */
        .king_attacks         = 0x0000000070507000ull, /* popcnt  8 */
    },
    { /* square g3 */
        .north                = 0x4040404040000000ull, /* popcnt  5 */
        .northeast            = 0x0000000080000000ull, /* popcnt  1 */
        .east                 = 0x0000000000800000ull, /* popcnt  1 */
        .southeast            = 0x0000000000008000ull, /* popcnt  1 */
        .south                = 0x0000000000004040ull, /* popcnt  2 */
        .southwest            = 0x0000000000002010ull, /* popcnt  2 */
        .west                 = 0x00000000003F0000ull, /* popcnt  6 */
        .northwest            = 0x0204081020000000ull, /* popcnt  5 */
        .pawn_attacks_white   = 0x00000000A0000000ull, /* popcnt  2 */
        .pawn_attacks_black   = 0x000000000000A000ull, /* popcnt  2 */
        .knight_attacks       = 0x000000A0100010A0ull, /* popcnt  6 */
        .bishop_attacks       = 0x02040810A000A010ull, /* popcnt  9 */
        .rook_attacks         = 0x4040404040BF4040ull, /* popcnt 14 */
        .queen_attacks        = 0x42444850E0BFE050ull, /* popcnt 23 */
        .king_attacks         = 0x00000000E0A0E000ull, /* popcnt  8 */
    },
    { /* square h3 */
        .north                = 0x8080808080000000ull, /* popcnt  5 */
        .northeast            = 0x0000000000000000ull, /* popcnt  0 */
        .east                 = 0x0000000000000000ull, /* popcnt  0 */
        .southeast            = 0x0000000000000000ull, /* popcnt  0 */
        .south                = 0x0000000000008080ull, /* popcnt  2 */
        .southwest            = 0x0000000000004020ull, /* popcnt  2 */
        .west                 = 0x00000000007F0000ull, /* popcnt  7 */
        .northwest            = 0x0408102040000000ull, /* popcnt  5 */
        .pawn_attacks_white   = 0x0000000040000000ull, /* popcnt  1 */
        .pawn_attacks_black   = 0x0000000000004000ull, /* popcnt  1 */
        .knight_attacks       = 0x0000004020002040ull, /* popcnt  4 */
        .bishop_attacks       = 0x0408102040004020ull, /* popcnt  7 */
        .rook_attacks         = 0x80808080807F8080ull, /* popcnt 14 */
        .queen_attacks        = 0x848890A0C07FC0A0ull, /* popcnt 21 */
        .king_attacks         = 0x00000000C040C000ull, /* popcnt  5 */
    },
    { /* square a4 */
        .north                = 0x0101010100000000ull, /* popcnt  4 */
        .northeast            = 0x1008040200000000ull, /* popcnt  4 */
        .east                 = 0x00000000FE000000ull, /* popcnt  7 */
        .southeast            = 0x0000000000020408ull, /* popcnt  3 */
        .south                = 0x0000000000010101ull, /* popcnt  3 */
        .southwest            = 0x0000000000000000ull, /* popcnt  0 */
        .west                 = 0x0000000000000000ull, /* popcnt  0 */
        .northwest            = 0x0000000000000000ull, /* popcnt  0 */
        .pawn_attacks_white   = 0x0000000200000000ull, /* popcnt  1 */
        .pawn_attacks_black   = 0x0000000000020000ull, /* popcnt  1 */
        .knight_attacks       = 0x0000020400040200ull, /* popcnt  4 */
        .bishop_attacks       = 0x1008040200020408ull, /* popcnt  7 */
        .rook_attacks         = 0x01010101FE010101ull, /* popcnt 14 */
        .queen_attacks        = 0x11090503FE030509ull, /* popcnt 21 */
        .king_attacks         = 0x0000000302030000ull, /* popcnt  5 */
    },
    { /* square b4 */
        .north                = 0x0202020200000000ull, /* popcnt  4 */
        .northeast            = 0x2010080400000000ull, /* popcnt  4 */
        .east                 = 0x00000000FC000000ull, /* popcnt  6 */
        .southeast            = 0x0000000000040810ull, /* popcnt  3 */
        .south                = 0x0000000000020202ull, /* popcnt  3 */
        .southwest            = 0x0000000000010000ull, /* popcnt  1 */
        .west                 = 0x0000000001000000ull, /* popcnt  1 */
        .northwest            = 0x0000000100000000ull, /* popcnt  1 */
        .pawn_attacks_white   = 0x0000000500000000ull, /* popcnt  2 */
        .pawn_attacks_black   = 0x0000000000050000ull, /* popcnt  2 */
        .knight_attacks       = 0x0000050800080500ull, /* popcnt  6 */
        .bishop_attacks       = 0x2010080500050810ull, /* popcnt  9 */
        .rook_attacks         = 0x02020202FD020202ull, /* popcnt 14 */
        .queen_attacks        = 0x22120A07FD070A12ull, /* popcnt 23 */
        .king_attacks         = 0x0000000705070000ull, /* popcnt  8 */
    },
    { /* square c4 */
        .north                = 0x0404040400000000ull, /* popcnt  4 */
        .northeast            = 0x4020100800000000ull, /* popcnt  4 */
        .east                 = 0x00000000F8000000ull, /* popcnt  5 */
        .southeast            = 0x0000000000081020ull, /* popcnt  3 */
        .south                = 0x0000000000040404ull, /* popcnt  3 */
        .southwest            = 0x0000000000020100ull, /* popcnt  2 */
        .west                 = 0x0000000003000000ull, /* popcnt  2 */
        .northwest            = 0x0000010200000000ull, /* popcnt  2 */
        .pawn_attacks_white   = 0x0000000A00000000ull, /* popcnt  2 */
        .pawn_attacks_black   = 0x00000000000A0000ull, /* popcnt  2 */
        .knight_attacks       = 0x00000A1100110A00ull, /* popcnt  8 */
        .bishop_attacks       = 0x4020110A000A1120ull, /* popcnt 11 */
        .rook_attacks         = 0x04040404FB040404ull, /* popcnt 14 */
        .queen_attacks        = 0x4424150EFB0E1524ull, /* popcnt 25 */
        .king_attacks         = 0x0000000E0A0E0000ull, /* popcnt  8 */
    },
    { /* square d4 */
        .north                = 0x0808080800000000ull, /* popcnt  4 */
        .northeast            = 0x8040201000000000ull, /* popcnt  4 */
        .east                 = 0x00000000F0000000ull, /* popcnt  4 */
        .southeast            = 0x0000000000102040ull, /* popcnt  3 */
        .south                = 0x0000000000080808ull, /* popcnt  3 */
        .southwest            = 0x0000000000040201ull, /* popcnt  3 */
        .west                 = 0x0000000007000000ull, /* popcnt  3 */
        .northwest            = 0x0001020400000000ull, /* popcnt  3 */
        .pawn_attacks_white   = 0x0000001400000000ull, /* popcnt  2 */
        .pawn_attacks_black   = 0x0000000000140000ull, /* popcnt  2 */
        .knight_attacks       = 0x0000142200221400ull, /* popcnt  8 */
        .bishop_attacks       = 0x8041221400142241ull, /* popcnt 13 */
        .rook_attacks         = 0x08080808F7080808ull, /* popcnt 14 */
        .queen_attacks        = 0x88492A1CF71C2A49ull, /* popcnt 27 */
        .king_attacks         = 0x0000001C141C0000ull, /* popcnt  8 */
    },
    { /* square e4 */
        .north                = 0x1010101000000000ull, /* popcnt  4 */
        .northeast            = 0x0080402000000000ull, /* popcnt  3 */
        .east                 = 0x00000000E0000000ull, /* popcnt  3 */
        .southeast            = 0x0000000000204080ull, /* popcnt  3 */
        .south                = 0x0000000000101010ull, /* popcnt  3 */
        .southwest            = 0x0000000000080402ull, /* popcnt  3 */
        .west                 = 0x000000000F000000ull, /* popcnt  4 */
        .northwest            = 0x0102040800000000ull, /* popcnt  4 */
        .pawn_attacks_white   = 0x0000002800000000ull, /* popcnt  2 */
        .pawn_attacks_black   = 0x0000000000280000ull, /* popcnt  2 */
        .knight_attacks       = 0x0000284400442800ull, /* popcnt  8 */
        .bishop_attacks       = 0x0182442800284482ull, /* popcnt 13 */
        .rook_attacks         = 0x10101010EF101010ull, /* popcnt 14 */
        .queen_attacks        = 0x11925438EF385492ull, /* popcnt 27 */
        .king_attacks         = 0x0000003828380000ull, /* popcnt  8 */
    },
    { /* square f4 */
        .north                = 0x2020202000000000ull, /* popcnt  4 */
        .northeast            = 0x0000804000000000ull, /* popcnt  2 */
        .east                 = 0x00000000C0000000ull, /* popcnt  2 */
        .southeast            = 0x0000000000408000ull, /* popcnt  2 */
        .south                = 0x0000000000202020ull, /* popcnt  3 */
        .southwest            = 0x0000000000100804ull, /* popcnt  3 */
        .west                 = 0x000000001F000000ull, /* popcnt  5 */
        .northwest            = 0x0204081000000000ull, /* popcnt  4 */
        .pawn_attacks_white   = 0x0000005000000000ull, /* popcnt  2 */
        .pawn_attacks_black   = 0x0000000000500000ull, /* popcnt  2 */
        .knight_attacks       = 0x0000508800885000ull, /* popcnt  8 */
        .bishop_attacks       = 0x0204885000508804ull, /* popcnt 11 */
        .rook_attacks         = 0x20202020DF202020ull, /* popcnt 14 */
        .queen_attacks        = 0x2224A870DF70A824ull, /* popcnt 25 */
        .king_attacks         = 0x0000007050700000ull, /* popcnt  8 */
    },
    { /* square g4 */
        .north                = 0x4040404000000000ull, /* popcnt  4 */
        .northeast            = 0x0000008000000000ull, /* popcnt  1 */
        .east                 = 0x0000000080000000ull, /* popcnt  1 */
        .southeast            = 0x0000000000800000ull, /* popcnt  1 */
        .south                = 0x0000000000404040ull, /* popcnt  3 */
        .southwest            = 0x0000000000201008ull, /* popcnt  3 */
        .west                 = 0x000000003F000000ull, /* popcnt  6 */
        .northwest            = 0x0408102000000000ull, /* popcnt  4 */
        .pawn_attacks_white   = 0x000000A000000000ull, /* popcnt  2 */
        .pawn_attacks_black   = 0x0000000000A00000ull, /* popcnt  2 */
        .knight_attacks       = 0x0000A0100010A000ull, /* popcnt  6 */
        .bishop_attacks       = 0x040810A000A01008ull, /* popcnt  9 */
        .rook_attacks         = 0x40404040BF404040ull, /* popcnt 14 */
        .queen_attacks        = 0x444850E0BFE05048ull, /* popcnt 23 */
        .king_attacks         = 0x000000E0A0E00000ull, /* popcnt  8 */
    },
    { /* square h4 */
        .north                = 0x8080808000000000ull, /* popcnt  4 */
        .northeast            = 0x0000000000000000ull, /* popcnt  0 */
        .east                 = 0x0000000000000000ull, /* popcnt  0 */
        .southeast            = 0x0000000000000000ull, /* popcnt  0 */
        .south                = 0x0000000000808080ull, /* popcnt  3 */
        .southwest            = 0x0000000000402010ull, /* popcnt  3 */
        .west                 = 0x000000007F000000ull, /* popcnt  7 */
        .northwest            = 0x0810204000000000ull, /* popcnt  4 */
        .pawn_attacks_white   = 0x0000004000000000ull, /* popcnt  1 */
        .pawn_attacks_black   = 0x0000000000400000ull, /* popcnt  1 */
        .knight_attacks       = 0x0000402000204000ull, /* popcnt  4 */
        .bishop_attacks       = 0x0810204000402010ull, /* popcnt  7 */
        .rook_attacks         = 0x808080807F808080ull, /* popcnt 14 */
        .queen_attacks        = 0x8890A0C07FC0A090ull, /* popcnt 21 */
        .king_attacks         = 0x000000C040C00000ull, /* popcnt  5 */
    },
    { /* square a5 */
        .north                = 0x0101010000000000ull, /* popcnt  3 */
        .northeast            = 0x0804020000000000ull, /* popcnt  3 */
        .east                 = 0x000000FE00000000ull, /* popcnt  7 */
        .southeast            = 0x0000000002040810ull, /* popcnt  4 */
        .south                = 0x0000000001010101ull, /* popcnt  4 */
        .southwest            = 0x0000000000000000ull, /* popcnt  0 */
        .west                 = 0x0000000000000000ull, /* popcnt  0 */
        .northwest            = 0x0000000000000000ull, /* popcnt  0 */
        .pawn_attacks_white   = 0x0000020000000000ull, /* popcnt  1 */
        .pawn_attacks_black   = 0x0000000002000000ull, /* popcnt  1 */
        .knight_attacks       = 0x0002040004020000ull, /* popcnt  4 */
        .bishop_attacks       = 0x0804020002040810ull, /* popcnt  7 */
        .rook_attacks         = 0x010101FE01010101ull, /* popcnt 14 */
        .queen_attacks        = 0x090503FE03050911ull, /* popcnt 21 */
        .king_attacks         = 0x0000030203000000ull, /* popcnt  5 */
    },
    { /* square b5 */
        .north                = 0x0202020000000000ull, /* popcnt  3 */
        .northeast            = 0x1008040000000000ull, /* popcnt  3 */
        .east                 = 0x000000FC00000000ull, /* popcnt  6 */
        .southeast            = 0x0000000004081020ull, /* popcnt  4 */
        .south                = 0x0000000002020202ull, /* popcnt  4 */
        .southwest            = 0x0000000001000000ull, /* popcnt  1 */
        .west                 = 0x0000000100000000ull, /* popcnt  1 */
        .northwest            = 0x0000010000000000ull, /* popcnt  1 */
        .pawn_attacks_white   = 0x0000050000000000ull, /* popcnt  2 */
        .pawn_attacks_black   = 0x0000000005000000ull, /* popcnt  2 */
        .knight_attacks       = 0x0005080008050000ull, /* popcnt  6 */
        .bishop_attacks       = 0x1008050005081020ull, /* popcnt  9 */
        .rook_attacks         = 0x020202FD02020202ull, /* popcnt 14 */
        .queen_attacks        = 0x120A07FD070A1222ull, /* popcnt 23 */
        .king_attacks         = 0x0000070507000000ull, /* popcnt  8 */
    },
    { /* square c5 */
        .north                = 0x0404040000000000ull, /* popcnt  3 */
        .northeast            = 0x2010080000000000ull, /* popcnt  3 */
        .east                 = 0x000000F800000000ull, /* popcnt  5 */
        .southeast            = 0x0000000008102040ull, /* popcnt  4 */
        .south                = 0x0000000004040404ull, /* popcnt  4 */
        .southwest            = 0x0000000002010000ull, /* popcnt  2 */
        .west                 = 0x0000000300000000ull, /* popcnt  2 */
        .northwest            = 0x0001020000000000ull, /* popcnt  2 */
        .pawn_attacks_white   = 0x00000A0000000000ull, /* popcnt  2 */
        .pawn_attacks_black   = 0x000000000A000000ull, /* popcnt  2 */
        .knight_attacks       = 0x000A1100110A0000ull, /* popcnt  8 */
        .bishop_attacks       = 0x20110A000A112040ull, /* popcnt 11 */
        .rook_attacks         = 0x040404FB04040404ull, /* popcnt 14 */
        .queen_attacks        = 0x24150EFB0E152444ull, /* popcnt 25 */
        .king_attacks         = 0x00000E0A0E000000ull, /* popcnt  8 */
    },
    { /* square d5 */
        .north                = 0x0808080000000000ull, /* popcnt  3 */
        .northeast            = 0x4020100000000000ull, /* popcnt  3 */
        .east                 = 0x000000F000000000ull, /* popcnt  4 */
        .southeast            = 0x0000000010204080ull, /* popcnt  4 */
        .south                = 0x0000000008080808ull, /* popcnt  4 */
        .southwest            = 0x0000000004020100ull, /* popcnt  3 */
        .west                 = 0x0000000700000000ull, /* popcnt  3 */
        .northwest            = 0x0102040000000000ull, /* popcnt  3 */
        .pawn_attacks_white   = 0x0000140000000000ull, /* popcnt  2 */
        .pawn_attacks_black   = 0x0000000014000000ull, /* popcnt  2 */
        .knight_attacks       = 0x0014220022140000ull, /* popcnt  8 */
        .bishop_attacks       = 0x4122140014224180ull, /* popcnt 13 */
        .rook_attacks         = 0x080808F708080808ull, /* popcnt 14 */
        .queen_attacks        = 0x492A1CF71C2A4988ull, /* popcnt 27 */
        .king_attacks         = 0x00001C141C000000ull, /* popcnt  8 */
    },
    { /* square e5 */
        .north                = 0x1010100000000000ull, /* popcnt  3 */
        .northeast            = 0x8040200000000000ull, /* popcnt  3 */
        .east                 = 0x000000E000000000ull, /* popcnt  3 */
        .southeast            = 0x0000000020408000ull, /* popcnt  3 */
        .south                = 0x0000000010101010ull, /* popcnt  4 */
        .southwest            = 0x0000000008040201ull, /* popcnt  4 */
        .west                 = 0x0000000F00000000ull, /* popcnt  4 */
        .northwest            = 0x0204080000000000ull, /* popcnt  3 */
        .pawn_attacks_white   = 0x0000280000000000ull, /* popcnt  2 */
        .pawn_attacks_black   = 0x0000000028000000ull, /* popcnt  2 */
        .knight_attacks       = 0x0028440044280000ull, /* popcnt  8 */
        .bishop_attacks       = 0x8244280028448201ull, /* popcnt 13 */
        .rook_attacks         = 0x101010EF10101010ull, /* popcnt 14 */
        .queen_attacks        = 0x925438EF38549211ull, /* popcnt 27 */
        .king_attacks         = 0x0000382838000000ull, /* popcnt  8 */
    },
    { /* square f5 */
        .north                = 0x2020200000000000ull, /* popcnt  3 */
        .northeast            = 0x0080400000000000ull, /* popcnt  2 */
        .east                 = 0x000000C000000000ull, /* popcnt  2 */
        .southeast            = 0x0000000040800000ull, /* popcnt  2 */
        .south                = 0x0000000020202020ull, /* popcnt  4 */
        .southwest            = 0x0000000010080402ull, /* popcnt  4 */
        .west                 = 0x0000001F00000000ull, /* popcnt  5 */
        .northwest            = 0x0408100000000000ull, /* popcnt  3 */
        .pawn_attacks_white   = 0x0000500000000000ull, /* popcnt  2 */
        .pawn_attacks_black   = 0x0000000050000000ull, /* popcnt  2 */
        .knight_attacks       = 0x0050880088500000ull, /* popcnt  8 */
        .bishop_attacks       = 0x0488500050880402ull, /* popcnt 11 */
        .rook_attacks         = 0x202020DF20202020ull, /* popcnt 14 */
        .queen_attacks        = 0x24A870DF70A82422ull, /* popcnt 25 */
        .king_attacks         = 0x0000705070000000ull, /* popcnt  8 */
    },
    { /* square g5 */
        .north                = 0x4040400000000000ull, /* popcnt  3 */
        .northeast            = 0x0000800000000000ull, /* popcnt  1 */
        .east                 = 0x0000008000000000ull, /* popcnt  1 */
        .southeast            = 0x0000000080000000ull, /* popcnt  1 */
        .south                = 0x0000000040404040ull, /* popcnt  4 */
        .southwest            = 0x0000000020100804ull, /* popcnt  4 */
        .west                 = 0x0000003F00000000ull, /* popcnt  6 */
        .northwest            = 0x0810200000000000ull, /* popcnt  3 */
        .pawn_attacks_white   = 0x0000A00000000000ull, /* popcnt  2 */
        .pawn_attacks_black   = 0x00000000A0000000ull, /* popcnt  2 */
        .knight_attacks       = 0x00A0100010A00000ull, /* popcnt  6 */
        .bishop_attacks       = 0x0810A000A0100804ull, /* popcnt  9 */
        .rook_attacks         = 0x404040BF40404040ull, /* popcnt 14 */
        .queen_attacks        = 0x4850E0BFE0504844ull, /* popcnt 23 */
        .king_attacks         = 0x0000E0A0E0000000ull, /* popcnt  8 */
    },
    { /* square h5 */
        .north                = 0x8080800000000000ull, /* popcnt  3 */
        .northeast            = 0x0000000000000000ull, /* popcnt  0 */
        .east                 = 0x0000000000000000ull, /* popcnt  0 */
        .southeast            = 0x0000000000000000ull, /* popcnt  0 */
        .south                = 0x0000000080808080ull, /* popcnt  4 */
        .southwest            = 0x0000000040201008ull, /* popcnt  4 */
        .west                 = 0x0000007F00000000ull, /* popcnt  7 */
        .northwest            = 0x1020400000000000ull, /* popcnt  3 */
        .pawn_attacks_white   = 0x0000400000000000ull, /* popcnt  1 */
        .pawn_attacks_black   = 0x0000000040000000ull, /* popcnt  1 */
        .knight_attacks       = 0x0040200020400000ull, /* popcnt  4 */
        .bishop_attacks       = 0x1020400040201008ull, /* popcnt  7 */
        .rook_attacks         = 0x8080807F80808080ull, /* popcnt 14 */
        .queen_attacks        = 0x90A0C07FC0A09088ull, /* popcnt 21 */
        .king_attacks         = 0x0000C040C0000000ull, /* popcnt  5 */
    },
    { /* square a6 */
        .north                = 0x0101000000000000ull, /* popcnt  2 */
        .northeast            = 0x0402000000000000ull, /* popcnt  2 */
        .east                 = 0x0000FE0000000000ull, /* popcnt  7 */
        .southeast            = 0x0000000204081020ull, /* popcnt  5 */
        .south                = 0x0000000101010101ull, /* popcnt  5 */
        .southwest            = 0x0000000000000000ull, /* popcnt  0 */
        .west                 = 0x0000000000000000ull, /* popcnt  0 */
        .northwest            = 0x0000000000000000ull, /* popcnt  0 */
        .pawn_attacks_white   = 0x0002000000000000ull, /* popcnt  1 */
        .pawn_attacks_black   = 0x0000000200000000ull, /* popcnt  1 */
        .knight_attacks       = 0x0204000402000000ull, /* popcnt  4 */
        .bishop_attacks       = 0x0402000204081020ull, /* popcnt  7 */
        .rook_attacks         = 0x0101FE0101010101ull, /* popcnt 14 */
        .queen_attacks        = 0x0503FE0305091121ull, /* popcnt 21 */
        .king_attacks         = 0x0003020300000000ull, /* popcnt  5 */
    },
    { /* square b6 */
        .north                = 0x0202000000000000ull, /* popcnt  2 */
        .northeast            = 0x0804000000000000ull, /* popcnt  2 */
        .east                 = 0x0000FC0000000000ull, /* popcnt  6 */
        .southeast            = 0x0000000408102040ull, /* popcnt  5 */
        .south                = 0x0000000202020202ull, /* popcnt  5 */
        .southwest            = 0x0000000100000000ull, /* popcnt  1 */
        .west                 = 0x0000010000000000ull, /* popcnt  1 */
        .northwest            = 0x0001000000000000ull, /* popcnt  1 */
        .pawn_attacks_white   = 0x0005000000000000ull, /* popcnt  2 */
        .pawn_attacks_black   = 0x0000000500000000ull, /* popcnt  2 */
        .knight_attacks       = 0x0508000805000000ull, /* popcnt  6 */
        .bishop_attacks       = 0x0805000508102040ull, /* popcnt  9 */
        .rook_attacks         = 0x0202FD0202020202ull, /* popcnt 14 */
        .queen_attacks        = 0x0A07FD070A122242ull, /* popcnt 23 */
        .king_attacks         = 0x0007050700000000ull, /* popcnt  8 */
    },
    { /* square c6 */
        .north                = 0x0404000000000000ull, /* popcnt  2 */
        .northeast            = 0x1008000000000000ull, /* popcnt  2 */
        .east                 = 0x0000F80000000000ull, /* popcnt  5 */
        .southeast            = 0x0000000810204080ull, /* popcnt  5 */
        .south                = 0x0000000404040404ull, /* popcnt  5 */
        .southwest            = 0x0000000201000000ull, /* popcnt  2 */
        .west                 = 0x0000030000000000ull, /* popcnt  2 */
        .northwest            = 0x0102000000000000ull, /* popcnt  2 */
        .pawn_attacks_white   = 0x000A000000000000ull, /* popcnt  2 */
        .pawn_attacks_black   = 0x0000000A00000000ull, /* popcnt  2 */
        .knight_attacks       = 0x0A1100110A000000ull, /* popcnt  8 */
        .bishop_attacks       = 0x110A000A11204080ull, /* popcnt 11 */
        .rook_attacks         = 0x0404FB0404040404ull, /* popcnt 14 */
        .queen_attacks        = 0x150EFB0E15244484ull, /* popcnt 25 */
        .king_attacks         = 0x000E0A0E00000000ull, /* popcnt  8 */
    },
    { /* square d6 */
        .north                = 0x0808000000000000ull, /* popcnt  2 */
        .northeast            = 0x2010000000000000ull, /* popcnt  2 */
        .east                 = 0x0000F00000000000ull, /* popcnt  4 */
        .southeast            = 0x0000001020408000ull, /* popcnt  4 */
        .south                = 0x0000000808080808ull, /* popcnt  5 */
        .southwest            = 0x0000000402010000ull, /* popcnt  3 */
        .west                 = 0x0000070000000000ull, /* popcnt  3 */
        .northwest            = 0x0204000000000000ull, /* popcnt  2 */
        .pawn_attacks_white   = 0x0014000000000000ull, /* popcnt  2 */
        .pawn_attacks_black   = 0x0000001400000000ull, /* popcnt  2 */
        .knight_attacks       = 0x1422002214000000ull, /* popcnt  8 */
        .bishop_attacks       = 0x2214001422418000ull, /* popcnt 11 */
        .rook_attacks         = 0x0808F70808080808ull, /* popcnt 14 */
        .queen_attacks        = 0x2A1CF71C2A498808ull, /* popcnt 25 */
        .king_attacks         = 0x001C141C00000000ull, /* popcnt  8 */
    },
    { /* square e6 */
        .north                = 0x1010000000000000ull, /* popcnt  2 */
        .northeast            = 0x4020000000000000ull, /* popcnt  2 */
        .east                 = 0x0000E00000000000ull, /* popcnt  3 */
        .southeast            = 0x0000002040800000ull, /* popcnt  3 */
        .south                = 0x0000001010101010ull, /* popcnt  5 */
        .southwest            = 0x0000000804020100ull, /* popcnt  4 */
        .west                 = 0x00000F0000000000ull, /* popcnt  4 */
        .northwest            = 0x0408000000000000ull, /* popcnt  2 */
        .pawn_attacks_white   = 0x0028000000000000ull, /* popcnt  2 */
        .pawn_attacks_black   = 0x0000002800000000ull, /* popcnt  2 */
        .knight_attacks       = 0x2844004428000000ull, /* popcnt  8 */
        .bishop_attacks       = 0x4428002844820100ull, /* popcnt 11 */
        .rook_attacks         = 0x1010EF1010101010ull, /* popcnt 14 */
        .queen_attacks        = 0x5438EF3854921110ull, /* popcnt 25 */
        .king_attacks         = 0x0038283800000000ull, /* popcnt  8 */
    },
    { /* square f6 */
        .north                = 0x2020000000000000ull, /* popcnt  2 */
        .northeast            = 0x8040000000000000ull, /* popcnt  2 */
        .east                 = 0x0000C00000000000ull, /* popcnt  2 */
        .southeast            = 0x0000004080000000ull, /* popcnt  2 */
        .south                = 0x0000002020202020ull, /* popcnt  5 */
        .southwest            = 0x0000001008040201ull, /* popcnt  5 */
        .west                 = 0x00001F0000000000ull, /* popcnt  5 */
        .northwest            = 0x0810000000000000ull, /* popcnt  2 */
        .pawn_attacks_white   = 0x0050000000000000ull, /* popcnt  2 */
        .pawn_attacks_black   = 0x0000005000000000ull, /* popcnt  2 */
        .knight_attacks       = 0x5088008850000000ull, /* popcnt  8 */
        .bishop_attacks       = 0x8850005088040201ull, /* popcnt 11 */
        .rook_attacks         = 0x2020DF2020202020ull, /* popcnt 14 */
        .queen_attacks        = 0xA870DF70A8242221ull, /* popcnt 25 */
        .king_attacks         = 0x0070507000000000ull, /* popcnt  8 */
    },
    { /* square g6 */
        .north                = 0x4040000000000000ull, /* popcnt  2 */
        .northeast            = 0x0080000000000000ull, /* popcnt  1 */
        .east                 = 0x0000800000000000ull, /* popcnt  1 */
        .southeast            = 0x0000008000000000ull, /* popcnt  1 */
        .south                = 0x0000004040404040ull, /* popcnt  5 */
        .southwest            = 0x0000002010080402ull, /* popcnt  5 */
        .west                 = 0x00003F0000000000ull, /* popcnt  6 */
        .northwest            = 0x1020000000000000ull, /* popcnt  2 */
        .pawn_attacks_white   = 0x00A0000000000000ull, /* popcnt  2 */
        .pawn_attacks_black   = 0x000000A000000000ull, /* popcnt  2 */
        .knight_attacks       = 0xA0100010A0000000ull, /* popcnt  6 */
        .bishop_attacks       = 0x10A000A010080402ull, /* popcnt  9 */
        .rook_attacks         = 0x4040BF4040404040ull, /* popcnt 14 */
        .queen_attacks        = 0x50E0BFE050484442ull, /* popcnt 23 */
        .king_attacks         = 0x00E0A0E000000000ull, /* popcnt  8 */
    },
    { /* square h6 */
        .north                = 0x8080000000000000ull, /* popcnt  2 */
        .northeast            = 0x0000000000000000ull, /* popcnt  0 */
        .east                 = 0x0000000000000000ull, /* popcnt  0 */
        .southeast            = 0x0000000000000000ull, /* popcnt  0 */
        .south                = 0x0000008080808080ull, /* popcnt  5 */
        .southwest            = 0x0000004020100804ull, /* popcnt  5 */
        .west                 = 0x00007F0000000000ull, /* popcnt  7 */
        .northwest            = 0x2040000000000000ull, /* popcnt  2 */
        .pawn_attacks_white   = 0x0040000000000000ull, /* popcnt  1 */
        .pawn_attacks_black   = 0x0000004000000000ull, /* popcnt  1 */
        .knight_attacks       = 0x4020002040000000ull, /* popcnt  4 */
        .bishop_attacks       = 0x2040004020100804ull, /* popcnt  7 */
        .rook_attacks         = 0x80807F8080808080ull, /* popcnt 14 */
        .queen_attacks        = 0xA0C07FC0A0908884ull, /* popcnt 21 */
        .king_attacks         = 0x00C040C000000000ull, /* popcnt  5 */
    },
    { /* square a7 */
        .north                = 0x0100000000000000ull, /* popcnt  1 */
        .northeast            = 0x0200000000000000ull, /* popcnt  1 */
        .east                 = 0x00FE000000000000ull, /* popcnt  7 */
        .southeast            = 0x0000020408102040ull, /* popcnt  6 */
        .south                = 0x0000010101010101ull, /* popcnt  6 */
        .southwest            = 0x0000000000000000ull, /* popcnt  0 */
        .west                 = 0x0000000000000000ull, /* popcnt  0 */
        .northwest            = 0x0000000000000000ull, /* popcnt  0 */
        .pawn_attacks_white   = 0x0200000000000000ull, /* popcnt  1 */
        .pawn_attacks_black   = 0x0000020000000000ull, /* popcnt  1 */
        .knight_attacks       = 0x0400040200000000ull, /* popcnt  3 */
        .bishop_attacks       = 0x0200020408102040ull, /* popcnt  7 */
        .rook_attacks         = 0x01FE010101010101ull, /* popcnt 14 */
        .queen_attacks        = 0x03FE030509112141ull, /* popcnt 21 */
        .king_attacks         = 0x0302030000000000ull, /* popcnt  5 */
    },
    { /* square b7 */
        .north                = 0x0200000000000000ull, /* popcnt  1 */
        .northeast            = 0x0400000000000000ull, /* popcnt  1 */
        .east                 = 0x00FC000000000000ull, /* popcnt  6 */
        .southeast            = 0x0000040810204080ull, /* popcnt  6 */
        .south                = 0x0000020202020202ull, /* popcnt  6 */
        .southwest            = 0x0000010000000000ull, /* popcnt  1 */
        .west                 = 0x0001000000000000ull, /* popcnt  1 */
        .northwest            = 0x0100000000000000ull, /* popcnt  1 */
        .pawn_attacks_white   = 0x0500000000000000ull, /* popcnt  2 */
        .pawn_attacks_black   = 0x0000050000000000ull, /* popcnt  2 */
        .knight_attacks       = 0x0800080500000000ull, /* popcnt  4 */
        .bishop_attacks       = 0x0500050810204080ull, /* popcnt  9 */
        .rook_attacks         = 0x02FD020202020202ull, /* popcnt 14 */
        .queen_attacks        = 0x07FD070A12224282ull, /* popcnt 23 */
        .king_attacks         = 0x0705070000000000ull, /* popcnt  8 */
    },
    { /* square c7 */
        .north                = 0x0400000000000000ull, /* popcnt  1 */
        .northeast            = 0x0800000000000000ull, /* popcnt  1 */
        .east                 = 0x00F8000000000000ull, /* popcnt  5 */
        .southeast            = 0x0000081020408000ull, /* popcnt  5 */
        .south                = 0x0000040404040404ull, /* popcnt  6 */
        .southwest            = 0x0000020100000000ull, /* popcnt  2 */
        .west                 = 0x0003000000000000ull, /* popcnt  2 */
        .northwest            = 0x0200000000000000ull, /* popcnt  1 */
        .pawn_attacks_white   = 0x0A00000000000000ull, /* popcnt  2 */
        .pawn_attacks_black   = 0x00000A0000000000ull, /* popcnt  2 */
        .knight_attacks       = 0x1100110A00000000ull, /* popcnt  6 */
        .bishop_attacks       = 0x0A000A1120408000ull, /* popcnt  9 */
        .rook_attacks         = 0x04FB040404040404ull, /* popcnt 14 */
        .queen_attacks        = 0x0EFB0E1524448404ull, /* popcnt 23 */
        .king_attacks         = 0x0E0A0E0000000000ull, /* popcnt  8 */
    },
    { /* square d7 */
        .north                = 0x0800000000000000ull, /* popcnt  1 */
        .northeast            = 0x1000000000000000ull, /* popcnt  1 */
        .east                 = 0x00F0000000000000ull, /* popcnt  4 */
        .southeast            = 0x0000102040800000ull, /* popcnt  4 */
        .south                = 0x0000080808080808ull, /* popcnt  6 */
        .southwest            = 0x0000040201000000ull, /* popcnt  3 */
        .west                 = 0x0007000000000000ull, /* popcnt  3 */
        .northwest            = 0x0400000000000000ull, /* popcnt  1 */
        .pawn_attacks_white   = 0x1400000000000000ull, /* popcnt  2 */
        .pawn_attacks_black   = 0x0000140000000000ull, /* popcnt  2 */
        .knight_attacks       = 0x2200221400000000ull, /* popcnt  6 */
        .bishop_attacks       = 0x1400142241800000ull, /* popcnt  9 */
        .rook_attacks         = 0x08F7080808080808ull, /* popcnt 14 */
        .queen_attacks        = 0x1CF71C2A49880808ull, /* popcnt 23 */
        .king_attacks         = 0x1C141C0000000000ull, /* popcnt  8 */
    },
    { /* square e7 */
        .north                = 0x1000000000000000ull, /* popcnt  1 */
        .northeast            = 0x2000000000000000ull, /* popcnt  1 */
        .east                 = 0x00E0000000000000ull, /* popcnt  3 */
        .southeast            = 0x0000204080000000ull, /* popcnt  3 */
        .south                = 0x0000101010101010ull, /* popcnt  6 */
        .southwest            = 0x0000080402010000ull, /* popcnt  4 */
        .west                 = 0x000F000000000000ull, /* popcnt  4 */
        .northwest            = 0x0800000000000000ull, /* popcnt  1 */
        .pawn_attacks_white   = 0x2800000000000000ull, /* popcnt  2 */
        .pawn_attacks_black   = 0x0000280000000000ull, /* popcnt  2 */
        .knight_attacks       = 0x4400442800000000ull, /* popcnt  6 */
        .bishop_attacks       = 0x2800284482010000ull, /* popcnt  9 */
        .rook_attacks         = 0x10EF101010101010ull, /* popcnt 14 */
        .queen_attacks        = 0x38EF385492111010ull, /* popcnt 23 */
        .king_attacks         = 0x3828380000000000ull, /* popcnt  8 */
    },
    { /* square f7 */
        .north                = 0x2000000000000000ull, /* popcnt  1 */
        .northeast            = 0x4000000000000000ull, /* popcnt  1 */
        .east                 = 0x00C0000000000000ull, /* popcnt  2 */
        .southeast            = 0x0000408000000000ull, /* popcnt  2 */
        .south                = 0x0000202020202020ull, /* popcnt  6 */
        .southwest            = 0x0000100804020100ull, /* popcnt  5 */
        .west                 = 0x001F000000000000ull, /* popcnt  5 */
        .northwest            = 0x1000000000000000ull, /* popcnt  1 */
        .pawn_attacks_white   = 0x5000000000000000ull, /* popcnt  2 */
        .pawn_attacks_black   = 0x0000500000000000ull, /* popcnt  2 */
        .knight_attacks       = 0x8800885000000000ull, /* popcnt  6 */
        .bishop_attacks       = 0x5000508804020100ull, /* popcnt  9 */
        .rook_attacks         = 0x20DF202020202020ull, /* popcnt 14 */
        .queen_attacks        = 0x70DF70A824222120ull, /* popcnt 23 */
        .king_attacks         = 0x7050700000000000ull, /* popcnt  8 */
    },
    { /* square g7 */
        .north                = 0x4000000000000000ull, /* popcnt  1 */
        .northeast            = 0x8000000000000000ull, /* popcnt  1 */
        .east                 = 0x0080000000000000ull, /* popcnt  1 */
        .southeast            = 0x0000800000000000ull, /* popcnt  1 */
        .south                = 0x0000404040404040ull, /* popcnt  6 */
        .southwest            = 0x0000201008040201ull, /* popcnt  6 */
        .west                 = 0x003F000000000000ull, /* popcnt  6 */
        .northwest            = 0x2000000000000000ull, /* popcnt  1 */
        .pawn_attacks_white   = 0xA000000000000000ull, /* popcnt  2 */
        .pawn_attacks_black   = 0x0000A00000000000ull, /* popcnt  2 */
        .knight_attacks       = 0x100010A000000000ull, /* popcnt  4 */
        .bishop_attacks       = 0xA000A01008040201ull, /* popcnt  9 */
        .rook_attacks         = 0x40BF404040404040ull, /* popcnt 14 */
        .queen_attacks        = 0xE0BFE05048444241ull, /* popcnt 23 */
        .king_attacks         = 0xE0A0E00000000000ull, /* popcnt  8 */
    },
    { /* square h7 */
        .north                = 0x8000000000000000ull, /* popcnt  1 */
        .northeast            = 0x0000000000000000ull, /* popcnt  0 */
        .east                 = 0x0000000000000000ull, /* popcnt  0 */
        .southeast            = 0x0000000000000000ull, /* popcnt  0 */
        .south                = 0x0000808080808080ull, /* popcnt  6 */
        .southwest            = 0x0000402010080402ull, /* popcnt  6 */
        .west                 = 0x007F000000000000ull, /* popcnt  7 */
        .northwest            = 0x4000000000000000ull, /* popcnt  1 */
        .pawn_attacks_white   = 0x4000000000000000ull, /* popcnt  1 */
        .pawn_attacks_black   = 0x0000400000000000ull, /* popcnt  1 */
        .knight_attacks       = 0x2000204000000000ull, /* popcnt  3 */
        .bishop_attacks       = 0x4000402010080402ull, /* popcnt  7 */
        .rook_attacks         = 0x807F808080808080ull, /* popcnt 14 */
        .queen_attacks        = 0xC07FC0A090888482ull, /* popcnt 21 */
        .king_attacks         = 0xC040C00000000000ull, /* popcnt  5 */
    },
    { /* square a8 */
        .north                = 0x0000000000000000ull, /* popcnt  0 */
        .northeast            = 0x0000000000000000ull, /* popcnt  0 */
        .east                 = 0xFE00000000000000ull, /* popcnt  7 */
        .southeast            = 0x0002040810204080ull, /* popcnt  7 */
        .south                = 0x0001010101010101ull, /* popcnt  7 */
        .southwest            = 0x0000000000000000ull, /* popcnt  0 */
        .west                 = 0x0000000000000000ull, /* popcnt  0 */
        .northwest            = 0x0000000000000000ull, /* popcnt  0 */
        .pawn_attacks_white   = 0x0000000000000000ull, /* popcnt  0 */
        .pawn_attacks_black   = 0x0002000000000000ull, /* popcnt  1 */
        .knight_attacks       = 0x0004020000000000ull, /* popcnt  2 */
        .bishop_attacks       = 0x0002040810204080ull, /* popcnt  7 */
        .rook_attacks         = 0xFE01010101010101ull, /* popcnt 14 */
        .queen_attacks        = 0xFE03050911214181ull, /* popcnt 21 */
        .king_attacks         = 0x0203000000000000ull, /* popcnt  3 */
    },
    { /* square b8 */
        .north                = 0x0000000000000000ull, /* popcnt  0 */
        .northeast            = 0x0000000000000000ull, /* popcnt  0 */
        .east                 = 0xFC00000000000000ull, /* popcnt  6 */
        .southeast            = 0x0004081020408000ull, /* popcnt  6 */
        .south                = 0x0002020202020202ull, /* popcnt  7 */
        .southwest            = 0x0001000000000000ull, /* popcnt  1 */
        .west                 = 0x0100000000000000ull, /* popcnt  1 */
        .northwest            = 0x0000000000000000ull, /* popcnt  0 */
        .pawn_attacks_white   = 0x0000000000000000ull, /* popcnt  0 */
        .pawn_attacks_black   = 0x0005000000000000ull, /* popcnt  2 */
        .knight_attacks       = 0x0008050000000000ull, /* popcnt  3 */
        .bishop_attacks       = 0x0005081020408000ull, /* popcnt  7 */
        .rook_attacks         = 0xFD02020202020202ull, /* popcnt 14 */
        .queen_attacks        = 0xFD070A1222428202ull, /* popcnt 21 */
        .king_attacks         = 0x0507000000000000ull, /* popcnt  5 */
    },
    { /* square c8 */
        .north                = 0x0000000000000000ull, /* popcnt  0 */
        .northeast            = 0x0000000000000000ull, /* popcnt  0 */
        .east                 = 0xF800000000000000ull, /* popcnt  5 */
        .southeast            = 0x0008102040800000ull, /* popcnt  5 */
        .south                = 0x0004040404040404ull, /* popcnt  7 */
        .southwest            = 0x0002010000000000ull, /* popcnt  2 */
        .west                 = 0x0300000000000000ull, /* popcnt  2 */
        .northwest            = 0x0000000000000000ull, /* popcnt  0 */
        .pawn_attacks_white   = 0x0000000000000000ull, /* popcnt  0 */
        .pawn_attacks_black   = 0x000A000000000000ull, /* popcnt  2 */
        .knight_attacks       = 0x00110A0000000000ull, /* popcnt  4 */
        .bishop_attacks       = 0x000A112040800000ull, /* popcnt  7 */
        .rook_attacks         = 0xFB04040404040404ull, /* popcnt 14 */
        .queen_attacks        = 0xFB0E152444840404ull, /* popcnt 21 */
        .king_attacks         = 0x0A0E000000000000ull, /* popcnt  5 */
    },
    { /* square d8 */
        .north                = 0x0000000000000000ull, /* popcnt  0 */
        .northeast            = 0x0000000000000000ull, /* popcnt  0 */
        .east                 = 0xF000000000000000ull, /* popcnt  4 */
        .southeast            = 0x0010204080000000ull, /* popcnt  4 */
        .south                = 0x0008080808080808ull, /* popcnt  7 */
        .southwest            = 0x0004020100000000ull, /* popcnt  3 */
        .west                 = 0x0700000000000000ull, /* popcnt  3 */
        .northwest            = 0x0000000000000000ull, /* popcnt  0 */
        .pawn_attacks_white   = 0x0000000000000000ull, /* popcnt  0 */
        .pawn_attacks_black   = 0x0014000000000000ull, /* popcnt  2 */
        .knight_attacks       = 0x0022140000000000ull, /* popcnt  4 */
        .bishop_attacks       = 0x0014224180000000ull, /* popcnt  7 */
        .rook_attacks         = 0xF708080808080808ull, /* popcnt 14 */
        .queen_attacks        = 0xF71C2A4988080808ull, /* popcnt 21 */
        .king_attacks         = 0x141C000000000000ull, /* popcnt  5 */
    },
    { /* square e8 */
        .north                = 0x0000000000000000ull, /* popcnt  0 */
        .northeast            = 0x0000000000000000ull, /* popcnt  0 */
        .east                 = 0xE000000000000000ull, /* popcnt  3 */
        .southeast            = 0x0020408000000000ull, /* popcnt  3 */
        .south                = 0x0010101010101010ull, /* popcnt  7 */
        .southwest            = 0x0008040201000000ull, /* popcnt  4 */
        .west                 = 0x0F00000000000000ull, /* popcnt  4 */
        .northwest            = 0x0000000000000000ull, /* popcnt  0 */
        .pawn_attacks_white   = 0x0000000000000000ull, /* popcnt  0 */
        .pawn_attacks_black   = 0x0028000000000000ull, /* popcnt  2 */
        .knight_attacks       = 0x0044280000000000ull, /* popcnt  4 */
        .bishop_attacks       = 0x0028448201000000ull, /* popcnt  7 */
        .rook_attacks         = 0xEF10101010101010ull, /* popcnt 14 */
        .queen_attacks        = 0xEF38549211101010ull, /* popcnt 21 */
        .king_attacks         = 0x2838000000000000ull, /* popcnt  5 */
    },
    { /* square f8 */
        .north                = 0x0000000000000000ull, /* popcnt  0 */
        .northeast            = 0x0000000000000000ull, /* popcnt  0 */
        .east                 = 0xC000000000000000ull, /* popcnt  2 */
        .southeast            = 0x0040800000000000ull, /* popcnt  2 */
        .south                = 0x0020202020202020ull, /* popcnt  7 */
        .southwest            = 0x0010080402010000ull, /* popcnt  5 */
        .west                 = 0x1F00000000000000ull, /* popcnt  5 */
        .northwest            = 0x0000000000000000ull, /* popcnt  0 */
        .pawn_attacks_white   = 0x0000000000000000ull, /* popcnt  0 */
        .pawn_attacks_black   = 0x0050000000000000ull, /* popcnt  2 */
        .knight_attacks       = 0x0088500000000000ull, /* popcnt  4 */
        .bishop_attacks       = 0x0050880402010000ull, /* popcnt  7 */
        .rook_attacks         = 0xDF20202020202020ull, /* popcnt 14 */
        .queen_attacks        = 0xDF70A82422212020ull, /* popcnt 21 */
        .king_attacks         = 0x5070000000000000ull, /* popcnt  5 */
    },
    { /* square g8 */
        .north                = 0x0000000000000000ull, /* popcnt  0 */
        .northeast            = 0x0000000000000000ull, /* popcnt  0 */
        .east                 = 0x8000000000000000ull, /* popcnt  1 */
        .southeast            = 0x0080000000000000ull, /* popcnt  1 */
        .south                = 0x0040404040404040ull, /* popcnt  7 */
        .southwest            = 0x0020100804020100ull, /* popcnt  6 */
        .west                 = 0x3F00000000000000ull, /* popcnt  6 */
        .northwest            = 0x0000000000000000ull, /* popcnt  0 */
        .pawn_attacks_white   = 0x0000000000000000ull, /* popcnt  0 */
        .pawn_attacks_black   = 0x00A0000000000000ull, /* popcnt  2 */
        .knight_attacks       = 0x0010A00000000000ull, /* popcnt  3 */
        .bishop_attacks       = 0x00A0100804020100ull, /* popcnt  7 */
        .rook_attacks         = 0xBF40404040404040ull, /* popcnt 14 */
        .queen_attacks        = 0xBFE0504844424140ull, /* popcnt 21 */
        .king_attacks         = 0xA0E0000000000000ull, /* popcnt  5 */
    },
    { /* square h8 */
        .north                = 0x0000000000000000ull, /* popcnt  0 */
        .northeast            = 0x0000000000000000ull, /* popcnt  0 */
        .east                 = 0x0000000000000000ull, /* popcnt  0 */
        .southeast            = 0x0000000000000000ull, /* popcnt  0 */
        .south                = 0x0080808080808080ull, /* popcnt  7 */
        .southwest            = 0x0040201008040201ull, /* popcnt  7 */
        .west                 = 0x7F00000000000000ull, /* popcnt  7 */
        .northwest            = 0x0000000000000000ull, /* popcnt  0 */
        .pawn_attacks_white   = 0x0000000000000000ull, /* popcnt  0 */
        .pawn_attacks_black   = 0x0040000000000000ull, /* popcnt  1 */
        .knight_attacks       = 0x0020400000000000ull, /* popcnt  2 */
        .bishop_attacks       = 0x0040201008040201ull, /* popcnt  7 */
        .rook_attacks         = 0x7F80808080808080ull, /* popcnt 14 */
        .queen_attacks        = 0x7FC0A09088848281ull, /* popcnt 21 */
        .king_attacks         = 0x40C0000000000000ull, /* popcnt  3 */
    },
};
const PawnSets PAWN_SETS[2][64] = 
{
    {
        { /* white square a1 */
            .passed_pawn_mask             = 0x0303030303030300ull, /* popcnt 14 */
            .isolated_pawn_mask           = 0x0202020202020202ull, /* popcnt  8 */
            .supported_pawn_mask          = 0x0000000000000000ull, /* popcnt  0 */
            .doubled_pawn_mask            = 0x0101010101010100ull, /* popcnt  7 */
        },
        { /* white square b1 */
            .passed_pawn_mask             = 0x0707070707070700ull, /* popcnt 21 */
            .isolated_pawn_mask           = 0x0505050505050505ull, /* popcnt 16 */
            .supported_pawn_mask          = 0x0000000000000000ull, /* popcnt  0 */
            .doubled_pawn_mask            = 0x0202020202020200ull, /* popcnt  7 */
        },
        { /* white square c1 */
            .passed_pawn_mask             = 0x0E0E0E0E0E0E0E00ull, /* popcnt 21 */
            .isolated_pawn_mask           = 0x0A0A0A0A0A0A0A0Aull, /* popcnt 16 */
            .supported_pawn_mask          = 0x0000000000000000ull, /* popcnt  0 */
            .doubled_pawn_mask            = 0x0404040404040400ull, /* popcnt  7 */
        },
        { /* white square d1 */
            .passed_pawn_mask             = 0x1C1C1C1C1C1C1C00ull, /* popcnt 21 */
            .isolated_pawn_mask           = 0x1414141414141414ull, /* popcnt 16 */
            .supported_pawn_mask          = 0x0000000000000000ull, /* popcnt  0 */
            .doubled_pawn_mask            = 0x0808080808080800ull, /* popcnt  7 */
        },
        { /* white square e1 */
            .passed_pawn_mask             = 0x3838383838383800ull, /* popcnt 21 */
            .isolated_pawn_mask           = 0x2828282828282828ull, /* popcnt 16 */
            .supported_pawn_mask          = 0x0000000000000000ull, /* popcnt  0 */
            .doubled_pawn_mask            = 0x1010101010101000ull, /* popcnt  7 */
        },
        { /* white square f1 */
            .passed_pawn_mask             = 0x7070707070707000ull, /* popcnt 21 */
            .isolated_pawn_mask           = 0x5050505050505050ull, /* popcnt 16 */
            .supported_pawn_mask          = 0x0000000000000000ull, /* popcnt  0 */
            .doubled_pawn_mask            = 0x2020202020202000ull, /* popcnt  7 */
        },
        { /* white square g1 */
            .passed_pawn_mask             = 0xE0E0E0E0E0E0E000ull, /* popcnt 21 */
            .isolated_pawn_mask           = 0xA0A0A0A0A0A0A0A0ull, /* popcnt 16 */
            .supported_pawn_mask          = 0x0000000000000000ull, /* popcnt  0 */
            .doubled_pawn_mask            = 0x4040404040404000ull, /* popcnt  7 */
        },
        { /* white square h1 */
            .passed_pawn_mask             = 0xC0C0C0C0C0C0C000ull, /* popcnt 14 */
            .isolated_pawn_mask           = 0x4040404040404040ull, /* popcnt  8 */
            .supported_pawn_mask          = 0x0000000000000000ull, /* popcnt  0 */
            .doubled_pawn_mask            = 0x8080808080808000ull, /* popcnt  7 */
        },
        { /* white square a2 */
            .passed_pawn_mask             = 0x0303030303030000ull, /* popcnt 12 */
            .isolated_pawn_mask           = 0x0202020202020202ull, /* popcnt  8 */
            .supported_pawn_mask          = 0x0000000000000002ull, /* popcnt  1 */
            .doubled_pawn_mask            = 0x0101010101010000ull, /* popcnt  6 */
        },
        { /* white square b2 */
            .passed_pawn_mask             = 0x0707070707070000ull, /* popcnt 18 */
            .isolated_pawn_mask           = 0x0505050505050505ull, /* popcnt 16 */
            .supported_pawn_mask          = 0x0000000000000005ull, /* popcnt  2 */
            .doubled_pawn_mask            = 0x0202020202020000ull, /* popcnt  6 */
        },
        { /* white square c2 */
            .passed_pawn_mask             = 0x0E0E0E0E0E0E0000ull, /* popcnt 18 */
            .isolated_pawn_mask           = 0x0A0A0A0A0A0A0A0Aull, /* popcnt 16 */
            .supported_pawn_mask          = 0x000000000000000Aull, /* popcnt  2 */
            .doubled_pawn_mask            = 0x0404040404040000ull, /* popcnt  6 */
        },
        { /* white square d2 */
            .passed_pawn_mask             = 0x1C1C1C1C1C1C0000ull, /* popcnt 18 */
            .isolated_pawn_mask           = 0x1414141414141414ull, /* popcnt 16 */
            .supported_pawn_mask          = 0x0000000000000014ull, /* popcnt  2 */
            .doubled_pawn_mask            = 0x0808080808080000ull, /* popcnt  6 */
        },
        { /* white square e2 */
            .passed_pawn_mask             = 0x3838383838380000ull, /* popcnt 18 */
            .isolated_pawn_mask           = 0x2828282828282828ull, /* popcnt 16 */
            .supported_pawn_mask          = 0x0000000000000028ull, /* popcnt  2 */
            .doubled_pawn_mask            = 0x1010101010100000ull, /* popcnt  6 */
        },
        { /* white square f2 */
            .passed_pawn_mask             = 0x7070707070700000ull, /* popcnt 18 */
            .isolated_pawn_mask           = 0x5050505050505050ull, /* popcnt 16 */
            .supported_pawn_mask          = 0x0000000000000050ull, /* popcnt  2 */
            .doubled_pawn_mask            = 0x2020202020200000ull, /* popcnt  6 */
        },
        { /* white square g2 */
            .passed_pawn_mask             = 0xE0E0E0E0E0E00000ull, /* popcnt 18 */
            .isolated_pawn_mask           = 0xA0A0A0A0A0A0A0A0ull, /* popcnt 16 */
            .supported_pawn_mask          = 0x00000000000000A0ull, /* popcnt  2 */
            .doubled_pawn_mask            = 0x4040404040400000ull, /* popcnt  6 */
        },
        { /* white square h2 */
            .passed_pawn_mask             = 0xC0C0C0C0C0C00000ull, /* popcnt 12 */
            .isolated_pawn_mask           = 0x4040404040404040ull, /* popcnt  8 */
            .supported_pawn_mask          = 0x0000000000000040ull, /* popcnt  1 */
            .doubled_pawn_mask            = 0x8080808080800000ull, /* popcnt  6 */
        },
        { /* white square a3 */
            .passed_pawn_mask             = 0x0303030303000000ull, /* popcnt 10 */
            .isolated_pawn_mask           = 0x0202020202020202ull, /* popcnt  8 */
            .supported_pawn_mask          = 0x0000000000000202ull, /* popcnt  2 */
            .doubled_pawn_mask            = 0x0101010101000000ull, /* popcnt  5 */
        },
        { /* white square b3 */
            .passed_pawn_mask             = 0x0707070707000000ull, /* popcnt 15 */
            .isolated_pawn_mask           = 0x0505050505050505ull, /* popcnt 16 */
            .supported_pawn_mask          = 0x0000000000000505ull, /* popcnt  4 */
            .doubled_pawn_mask            = 0x0202020202000000ull, /* popcnt  5 */
        },
        { /* white square c3 */
            .passed_pawn_mask             = 0x0E0E0E0E0E000000ull, /* popcnt 15 */
            .isolated_pawn_mask           = 0x0A0A0A0A0A0A0A0Aull, /* popcnt 16 */
            .supported_pawn_mask          = 0x0000000000000A0Aull, /* popcnt  4 */
            .doubled_pawn_mask            = 0x0404040404000000ull, /* popcnt  5 */
        },
        { /* white square d3 */
            .passed_pawn_mask             = 0x1C1C1C1C1C000000ull, /* popcnt 15 */
            .isolated_pawn_mask           = 0x1414141414141414ull, /* popcnt 16 */
            .supported_pawn_mask          = 0x0000000000001414ull, /* popcnt  4 */
            .doubled_pawn_mask            = 0x0808080808000000ull, /* popcnt  5 */
        },
        { /* white square e3 */
            .passed_pawn_mask             = 0x3838383838000000ull, /* popcnt 15 */
            .isolated_pawn_mask           = 0x2828282828282828ull, /* popcnt 16 */
            .supported_pawn_mask          = 0x0000000000002828ull, /* popcnt  4 */
            .doubled_pawn_mask            = 0x1010101010000000ull, /* popcnt  5 */
        },
        { /* white square f3 */
            .passed_pawn_mask             = 0x7070707070000000ull, /* popcnt 15 */
            .isolated_pawn_mask           = 0x5050505050505050ull, /* popcnt 16 */
            .supported_pawn_mask          = 0x0000000000005050ull, /* popcnt  4 */
            .doubled_pawn_mask            = 0x2020202020000000ull, /* popcnt  5 */
        },
        { /* white square g3 */
            .passed_pawn_mask             = 0xE0E0E0E0E0000000ull, /* popcnt 15 */
            .isolated_pawn_mask           = 0xA0A0A0A0A0A0A0A0ull, /* popcnt 16 */
            .supported_pawn_mask          = 0x000000000000A0A0ull, /* popcnt  4 */
            .doubled_pawn_mask            = 0x4040404040000000ull, /* popcnt  5 */
        },
        { /* white square h3 */
            .passed_pawn_mask             = 0xC0C0C0C0C0000000ull, /* popcnt 10 */
            .isolated_pawn_mask           = 0x4040404040404040ull, /* popcnt  8 */
            .supported_pawn_mask          = 0x0000000000004040ull, /* popcnt  2 */
            .doubled_pawn_mask            = 0x8080808080000000ull, /* popcnt  5 */
        },
        { /* white square a4 */
            .passed_pawn_mask             = 0x0303030300000000ull, /* popcnt  8 */
            .isolated_pawn_mask           = 0x0202020202020202ull, /* popcnt  8 */
            .supported_pawn_mask          = 0x0000000000020202ull, /* popcnt  3 */
            .doubled_pawn_mask            = 0x0101010100000000ull, /* popcnt  4 */
        },
        { /* white square b4 */
            .passed_pawn_mask             = 0x0707070700000000ull, /* popcnt 12 */
            .isolated_pawn_mask           = 0x0505050505050505ull, /* popcnt 16 */
            .supported_pawn_mask          = 0x0000000000050505ull, /* popcnt  6 */
            .doubled_pawn_mask            = 0x0202020200000000ull, /* popcnt  4 */
        },
        { /* white square c4 */
            .passed_pawn_mask             = 0x0E0E0E0E00000000ull, /* popcnt 12 */
            .isolated_pawn_mask           = 0x0A0A0A0A0A0A0A0Aull, /* popcnt 16 */
            .supported_pawn_mask          = 0x00000000000A0A0Aull, /* popcnt  6 */
            .doubled_pawn_mask            = 0x0404040400000000ull, /* popcnt  4 */
        },
        { /* white square d4 */
            .passed_pawn_mask             = 0x1C1C1C1C00000000ull, /* popcnt 12 */
            .isolated_pawn_mask           = 0x1414141414141414ull, /* popcnt 16 */
            .supported_pawn_mask          = 0x0000000000141414ull, /* popcnt  6 */
            .doubled_pawn_mask            = 0x0808080800000000ull, /* popcnt  4 */
        },
        { /* white square e4 */
            .passed_pawn_mask             = 0x3838383800000000ull, /* popcnt 12 */
            .isolated_pawn_mask           = 0x2828282828282828ull, /* popcnt 16 */
            .supported_pawn_mask          = 0x0000000000282828ull, /* popcnt  6 */
            .doubled_pawn_mask            = 0x1010101000000000ull, /* popcnt  4 */
        },
        { /* white square f4 */
            .passed_pawn_mask             = 0x7070707000000000ull, /* popcnt 12 */
            .isolated_pawn_mask           = 0x5050505050505050ull, /* popcnt 16 */
            .supported_pawn_mask          = 0x0000000000505050ull, /* popcnt  6 */
            .doubled_pawn_mask            = 0x2020202000000000ull, /* popcnt  4 */
        },
        { /* white square g4 */
            .passed_pawn_mask             = 0xE0E0E0E000000000ull, /* popcnt 12 */
            .isolated_pawn_mask           = 0xA0A0A0A0A0A0A0A0ull, /* popcnt 16 */
            .supported_pawn_mask          = 0x0000000000A0A0A0ull, /* popcnt  6 */
            .doubled_pawn_mask            = 0x4040404000000000ull, /* popcnt  4 */
        },
        { /* white square h4 */
            .passed_pawn_mask             = 0xC0C0C0C000000000ull, /* popcnt  8 */
            .isolated_pawn_mask           = 0x4040404040404040ull, /* popcnt  8 */
            .supported_pawn_mask          = 0x0000000000404040ull, /* popcnt  3 */
            .doubled_pawn_mask            = 0x8080808000000000ull, /* popcnt  4 */
        },
        { /* white square a5 */
            .passed_pawn_mask             = 0x0303030000000000ull, /* popcnt  6 */
            .isolated_pawn_mask           = 0x0202020202020202ull, /* popcnt  8 */
            .supported_pawn_mask          = 0x0000000002020202ull, /* popcnt  4 */
            .doubled_pawn_mask            = 0x0101010000000000ull, /* popcnt  3 */
        },
        { /* white square b5 */
            .passed_pawn_mask             = 0x0707070000000000ull, /* popcnt  9 */
            .isolated_pawn_mask           = 0x0505050505050505ull, /* popcnt 16 */
            .supported_pawn_mask          = 0x0000000005050505ull, /* popcnt  8 */
            .doubled_pawn_mask            = 0x0202020000000000ull, /* popcnt  3 */
        },
        { /* white square c5 */
            .passed_pawn_mask             = 0x0E0E0E0000000000ull, /* popcnt  9 */
            .isolated_pawn_mask           = 0x0A0A0A0A0A0A0A0Aull, /* popcnt 16 */
            .supported_pawn_mask          = 0x000000000A0A0A0Aull, /* popcnt  8 */
            .doubled_pawn_mask            = 0x0404040000000000ull, /* popcnt  3 */
        },
        { /* white square d5 */
            .passed_pawn_mask             = 0x1C1C1C0000000000ull, /* popcnt  9 */
            .isolated_pawn_mask           = 0x1414141414141414ull, /* popcnt 16 */
            .supported_pawn_mask          = 0x0000000014141414ull, /* popcnt  8 */
            .doubled_pawn_mask            = 0x0808080000000000ull, /* popcnt  3 */
        },
        { /* white square e5 */
            .passed_pawn_mask             = 0x3838380000000000ull, /* popcnt  9 */
            .isolated_pawn_mask           = 0x2828282828282828ull, /* popcnt 16 */
            .supported_pawn_mask          = 0x0000000028282828ull, /* popcnt  8 */
            .doubled_pawn_mask            = 0x1010100000000000ull, /* popcnt  3 */
        },
        { /* white square f5 */
            .passed_pawn_mask             = 0x7070700000000000ull, /* popcnt  9 */
            .isolated_pawn_mask           = 0x5050505050505050ull, /* popcnt 16 */
            .supported_pawn_mask          = 0x0000000050505050ull, /* popcnt  8 */
            .doubled_pawn_mask            = 0x2020200000000000ull, /* popcnt  3 */
        },
        { /* white square g5 */
            .passed_pawn_mask             = 0xE0E0E00000000000ull, /* popcnt  9 */
            .isolated_pawn_mask           = 0xA0A0A0A0A0A0A0A0ull, /* popcnt 16 */
            .supported_pawn_mask          = 0x00000000A0A0A0A0ull, /* popcnt  8 */
            .doubled_pawn_mask            = 0x4040400000000000ull, /* popcnt  3 */
        },
        { /* white square h5 */
            .passed_pawn_mask             = 0xC0C0C00000000000ull, /* popcnt  6 */
            .isolated_pawn_mask           = 0x4040404040404040ull, /* popcnt  8 */
            .supported_pawn_mask          = 0x0000000040404040ull, /* popcnt  4 */
            .doubled_pawn_mask            = 0x8080800000000000ull, /* popcnt  3 */
        },
        { /* white square a6 */
            .passed_pawn_mask             = 0x0303000000000000ull, /* popcnt  4 */
            .isolated_pawn_mask           = 0x0202020202020202ull, /* popcnt  8 */
            .supported_pawn_mask          = 0x0000000202020202ull, /* popcnt  5 */
            .doubled_pawn_mask            = 0x0101000000000000ull, /* popcnt  2 */
        },
        { /* white square b6 */
            .passed_pawn_mask             = 0x0707000000000000ull, /* popcnt  6 */
            .isolated_pawn_mask           = 0x0505050505050505ull, /* popcnt 16 */
            .supported_pawn_mask          = 0x0000000505050505ull, /* popcnt 10 */
            .doubled_pawn_mask            = 0x0202000000000000ull, /* popcnt  2 */
        },
        { /* white square c6 */
            .passed_pawn_mask             = 0x0E0E000000000000ull, /* popcnt  6 */
            .isolated_pawn_mask           = 0x0A0A0A0A0A0A0A0Aull, /* popcnt 16 */
            .supported_pawn_mask          = 0x0000000A0A0A0A0Aull, /* popcnt 10 */
            .doubled_pawn_mask            = 0x0404000000000000ull, /* popcnt  2 */
        },
        { /* white square d6 */
            .passed_pawn_mask             = 0x1C1C000000000000ull, /* popcnt  6 */
            .isolated_pawn_mask           = 0x1414141414141414ull, /* popcnt 16 */
            .supported_pawn_mask          = 0x0000001414141414ull, /* popcnt 10 */
            .doubled_pawn_mask            = 0x0808000000000000ull, /* popcnt  2 */
        },
        { /* white square e6 */
            .passed_pawn_mask             = 0x3838000000000000ull, /* popcnt  6 */
            .isolated_pawn_mask           = 0x2828282828282828ull, /* popcnt 16 */
            .supported_pawn_mask          = 0x0000002828282828ull, /* popcnt 10 */
            .doubled_pawn_mask            = 0x1010000000000000ull, /* popcnt  2 */
        },
        { /* white square f6 */
            .passed_pawn_mask             = 0x7070000000000000ull, /* popcnt  6 */
            .isolated_pawn_mask           = 0x5050505050505050ull, /* popcnt 16 */
            .supported_pawn_mask          = 0x0000005050505050ull, /* popcnt 10 */
            .doubled_pawn_mask            = 0x2020000000000000ull, /* popcnt  2 */
        },
        { /* white square g6 */
            .passed_pawn_mask             = 0xE0E0000000000000ull, /* popcnt  6 */
            .isolated_pawn_mask           = 0xA0A0A0A0A0A0A0A0ull, /* popcnt 16 */
            .supported_pawn_mask          = 0x000000A0A0A0A0A0ull, /* popcnt 10 */
            .doubled_pawn_mask            = 0x4040000000000000ull, /* popcnt  2 */
        },
        { /* white square h6 */
            .passed_pawn_mask             = 0xC0C0000000000000ull, /* popcnt  4 */
            .isolated_pawn_mask           = 0x4040404040404040ull, /* popcnt  8 */
            .supported_pawn_mask          = 0x0000004040404040ull, /* popcnt  5 */
            .doubled_pawn_mask            = 0x8080000000000000ull, /* popcnt  2 */
        },
        { /* white square a7 */
            .passed_pawn_mask             = 0x0300000000000000ull, /* popcnt  2 */
            .isolated_pawn_mask           = 0x0202020202020202ull, /* popcnt  8 */
            .supported_pawn_mask          = 0x0000020202020202ull, /* popcnt  6 */
            .doubled_pawn_mask            = 0x0100000000000000ull, /* popcnt  1 */
        },
        { /* white square b7 */
            .passed_pawn_mask             = 0x0700000000000000ull, /* popcnt  3 */
            .isolated_pawn_mask           = 0x0505050505050505ull, /* popcnt 16 */
            .supported_pawn_mask          = 0x0000050505050505ull, /* popcnt 12 */
            .doubled_pawn_mask            = 0x0200000000000000ull, /* popcnt  1 */
        },
        { /* white square c7 */
            .passed_pawn_mask             = 0x0E00000000000000ull, /* popcnt  3 */
            .isolated_pawn_mask           = 0x0A0A0A0A0A0A0A0Aull, /* popcnt 16 */
            .supported_pawn_mask          = 0x00000A0A0A0A0A0Aull, /* popcnt 12 */
            .doubled_pawn_mask            = 0x0400000000000000ull, /* popcnt  1 */
        },
        { /* white square d7 */
            .passed_pawn_mask             = 0x1C00000000000000ull, /* popcnt  3 */
            .isolated_pawn_mask           = 0x1414141414141414ull, /* popcnt 16 */
            .supported_pawn_mask          = 0x0000141414141414ull, /* popcnt 12 */
            .doubled_pawn_mask            = 0x0800000000000000ull, /* popcnt  1 */
        },
        { /* white square e7 */
            .passed_pawn_mask             = 0x3800000000000000ull, /* popcnt  3 */
            .isolated_pawn_mask           = 0x2828282828282828ull, /* popcnt 16 */
            .supported_pawn_mask          = 0x0000282828282828ull, /* popcnt 12 */
            .doubled_pawn_mask            = 0x1000000000000000ull, /* popcnt  1 */
        },
        { /* white square f7 */
            .passed_pawn_mask             = 0x7000000000000000ull, /* popcnt  3 */
            .isolated_pawn_mask           = 0x5050505050505050ull, /* popcnt 16 */
            .supported_pawn_mask          = 0x0000505050505050ull, /* popcnt 12 */
            .doubled_pawn_mask            = 0x2000000000000000ull, /* popcnt  1 */
        },
        { /* white square g7 */
            .passed_pawn_mask             = 0xE000000000000000ull, /* popcnt  3 */
            .isolated_pawn_mask           = 0xA0A0A0A0A0A0A0A0ull, /* popcnt 16 */
            .supported_pawn_mask          = 0x0000A0A0A0A0A0A0ull, /* popcnt 12 */
            .doubled_pawn_mask            = 0x4000000000000000ull, /* popcnt  1 */
        },
        { /* white square h7 */
            .passed_pawn_mask             = 0xC000000000000000ull, /* popcnt  2 */
            .isolated_pawn_mask           = 0x4040404040404040ull, /* popcnt  8 */
            .supported_pawn_mask          = 0x0000404040404040ull, /* popcnt  6 */
            .doubled_pawn_mask            = 0x8000000000000000ull, /* popcnt  1 */
        },
        { /* white square a8 */
            .passed_pawn_mask             = 0x0000000000000000ull, /* popcnt  0 */
            .isolated_pawn_mask           = 0x0202020202020202ull, /* popcnt  8 */
            .supported_pawn_mask          = 0x0002020202020202ull, /* popcnt  7 */
            .doubled_pawn_mask            = 0x0000000000000000ull, /* popcnt  0 */
        },
        { /* white square b8 */
            .passed_pawn_mask             = 0x0000000000000000ull, /* popcnt  0 */
            .isolated_pawn_mask           = 0x0505050505050505ull, /* popcnt 16 */
            .supported_pawn_mask          = 0x0005050505050505ull, /* popcnt 14 */
            .doubled_pawn_mask            = 0x0000000000000000ull, /* popcnt  0 */
        },
        { /* white square c8 */
            .passed_pawn_mask             = 0x0000000000000000ull, /* popcnt  0 */
            .isolated_pawn_mask           = 0x0A0A0A0A0A0A0A0Aull, /* popcnt 16 */
            .supported_pawn_mask          = 0x000A0A0A0A0A0A0Aull, /* popcnt 14 */
            .doubled_pawn_mask            = 0x0000000000000000ull, /* popcnt  0 */
        },
        { /* white square d8 */
            .passed_pawn_mask             = 0x0000000000000000ull, /* popcnt  0 */
            .isolated_pawn_mask           = 0x1414141414141414ull, /* popcnt 16 */
            .supported_pawn_mask          = 0x0014141414141414ull, /* popcnt 14 */
            .doubled_pawn_mask            = 0x0000000000000000ull, /* popcnt  0 */
        },
        { /* white square e8 */
            .passed_pawn_mask             = 0x0000000000000000ull, /* popcnt  0 */
            .isolated_pawn_mask           = 0x2828282828282828ull, /* popcnt 16 */
            .supported_pawn_mask          = 0x0028282828282828ull, /* popcnt 14 */
            .doubled_pawn_mask            = 0x0000000000000000ull, /* popcnt  0 */
        },
        { /* white square f8 */
            .passed_pawn_mask             = 0x0000000000000000ull, /* popcnt  0 */
            .isolated_pawn_mask           = 0x5050505050505050ull, /* popcnt 16 */
            .supported_pawn_mask          = 0x0050505050505050ull, /* popcnt 14 */
            .doubled_pawn_mask            = 0x0000000000000000ull, /* popcnt  0 */
        },
        { /* white square g8 */
            .passed_pawn_mask             = 0x0000000000000000ull, /* popcnt  0 */
            .isolated_pawn_mask           = 0xA0A0A0A0A0A0A0A0ull, /* popcnt 16 */
            .supported_pawn_mask          = 0x00A0A0A0A0A0A0A0ull, /* popcnt 14 */
            .doubled_pawn_mask            = 0x0000000000000000ull, /* popcnt  0 */
        },
        { /* white square h8 */
            .passed_pawn_mask             = 0x0000000000000000ull, /* popcnt  0 */
            .isolated_pawn_mask           = 0x4040404040404040ull, /* popcnt  8 */
            .supported_pawn_mask          = 0x0040404040404040ull, /* popcnt  7 */
            .doubled_pawn_mask            = 0x0000000000000000ull, /* popcnt  0 */
        },
    },
    {
        { /* black square a1 */
            .passed_pawn_mask             = 0x0000000000000000ull, /* popcnt  0 */
            .isolated_pawn_mask           = 0x0202020202020202ull, /* popcnt  8 */
            .supported_pawn_mask          = 0x0202020202020200ull, /* popcnt  7 */
            .doubled_pawn_mask            = 0x0000000000000000ull, /* popcnt  0 */
        },
        { /* black square b1 */
            .passed_pawn_mask             = 0x0000000000000000ull, /* popcnt  0 */
            .isolated_pawn_mask           = 0x0505050505050505ull, /* popcnt 16 */
            .supported_pawn_mask          = 0x0505050505050500ull, /* popcnt 14 */
            .doubled_pawn_mask            = 0x0000000000000000ull, /* popcnt  0 */
        },
        { /* black square c1 */
            .passed_pawn_mask             = 0x0000000000000000ull, /* popcnt  0 */
            .isolated_pawn_mask           = 0x0A0A0A0A0A0A0A0Aull, /* popcnt 16 */
            .supported_pawn_mask          = 0x0A0A0A0A0A0A0A00ull, /* popcnt 14 */
            .doubled_pawn_mask            = 0x0000000000000000ull, /* popcnt  0 */
        },
        { /* black square d1 */
            .passed_pawn_mask             = 0x0000000000000000ull, /* popcnt  0 */
            .isolated_pawn_mask           = 0x1414141414141414ull, /* popcnt 16 */
            .supported_pawn_mask          = 0x1414141414141400ull, /* popcnt 14 */
            .doubled_pawn_mask            = 0x0000000000000000ull, /* popcnt  0 */
        },
        { /* black square e1 */
            .passed_pawn_mask             = 0x0000000000000000ull, /* popcnt  0 */
            .isolated_pawn_mask           = 0x2828282828282828ull, /* popcnt 16 */
            .supported_pawn_mask          = 0x2828282828282800ull, /* popcnt 14 */
            .doubled_pawn_mask            = 0x0000000000000000ull, /* popcnt  0 */
        },
        { /* black square f1 */
            .passed_pawn_mask             = 0x0000000000000000ull, /* popcnt  0 */
            .isolated_pawn_mask           = 0x5050505050505050ull, /* popcnt 16 */
            .supported_pawn_mask          = 0x5050505050505000ull, /* popcnt 14 */
            .doubled_pawn_mask            = 0x0000000000000000ull, /* popcnt  0 */
        },
        { /* black square g1 */
            .passed_pawn_mask             = 0x0000000000000000ull, /* popcnt  0 */
            .isolated_pawn_mask           = 0xA0A0A0A0A0A0A0A0ull, /* popcnt 16 */
            .supported_pawn_mask          = 0xA0A0A0A0A0A0A000ull, /* popcnt 14 */
            .doubled_pawn_mask            = 0x0000000000000000ull, /* popcnt  0 */
        },
        { /* black square h1 */
            .passed_pawn_mask             = 0x0000000000000000ull, /* popcnt  0 */
            .isolated_pawn_mask           = 0x4040404040404040ull, /* popcnt  8 */
            .supported_pawn_mask          = 0x4040404040404000ull, /* popcnt  7 */
            .doubled_pawn_mask            = 0x0000000000000000ull, /* popcnt  0 */
        },
        { /* black square a2 */
            .passed_pawn_mask             = 0x0000000000000003ull, /* popcnt  2 */
            .isolated_pawn_mask           = 0x0202020202020202ull, /* popcnt  8 */
            .supported_pawn_mask          = 0x0202020202020000ull, /* popcnt  6 */
            .doubled_pawn_mask            = 0x0000000000000001ull, /* popcnt  1 */
        },
        { /* black square b2 */
            .passed_pawn_mask             = 0x0000000000000007ull, /* popcnt  3 */
            .isolated_pawn_mask           = 0x0505050505050505ull, /* popcnt 16 */
            .supported_pawn_mask          = 0x0505050505050000ull, /* popcnt 12 */
            .doubled_pawn_mask            = 0x0000000000000002ull, /* popcnt  1 */
        },
        { /* black square c2 */
            .passed_pawn_mask             = 0x000000000000000Eull, /* popcnt  3 */
            .isolated_pawn_mask           = 0x0A0A0A0A0A0A0A0Aull, /* popcnt 16 */
            .supported_pawn_mask          = 0x0A0A0A0A0A0A0000ull, /* popcnt 12 */
            .doubled_pawn_mask            = 0x0000000000000004ull, /* popcnt  1 */
        },
        { /* black square d2 */
            .passed_pawn_mask             = 0x000000000000001Cull, /* popcnt  3 */
            .isolated_pawn_mask           = 0x1414141414141414ull, /* popcnt 16 */
            .supported_pawn_mask          = 0x1414141414140000ull, /* popcnt 12 */
            .doubled_pawn_mask            = 0x0000000000000008ull, /* popcnt  1 */
        },
        { /* black square e2 */
            .passed_pawn_mask             = 0x0000000000000038ull, /* popcnt  3 */
            .isolated_pawn_mask           = 0x2828282828282828ull, /* popcnt 16 */
            .supported_pawn_mask          = 0x2828282828280000ull, /* popcnt 12 */
            .doubled_pawn_mask            = 0x0000000000000010ull, /* popcnt  1 */
        },
        { /* black square f2 */
            .passed_pawn_mask             = 0x0000000000000070ull, /* popcnt  3 */
            .isolated_pawn_mask           = 0x5050505050505050ull, /* popcnt 16 */
            .supported_pawn_mask          = 0x5050505050500000ull, /* popcnt 12 */
            .doubled_pawn_mask            = 0x0000000000000020ull, /* popcnt  1 */
        },
        { /* black square g2 */
            .passed_pawn_mask             = 0x00000000000000E0ull, /* popcnt  3 */
            .isolated_pawn_mask           = 0xA0A0A0A0A0A0A0A0ull, /* popcnt 16 */
            .supported_pawn_mask          = 0xA0A0A0A0A0A00000ull, /* popcnt 12 */
            .doubled_pawn_mask            = 0x0000000000000040ull, /* popcnt  1 */
        },
        { /* black square h2 */
            .passed_pawn_mask             = 0x00000000000000C0ull, /* popcnt  2 */
            .isolated_pawn_mask           = 0x4040404040404040ull, /* popcnt  8 */
            .supported_pawn_mask          = 0x4040404040400000ull, /* popcnt  6 */
            .doubled_pawn_mask            = 0x0000000000000080ull, /* popcnt  1 */
        },
        { /* black square a3 */
            .passed_pawn_mask             = 0x0000000000000303ull, /* popcnt  4 */
            .isolated_pawn_mask           = 0x0202020202020202ull, /* popcnt  8 */
            .supported_pawn_mask          = 0x0202020202000000ull, /* popcnt  5 */
            .doubled_pawn_mask            = 0x0000000000000101ull, /* popcnt  2 */
        },
        { /* black square b3 */
            .passed_pawn_mask             = 0x0000000000000707ull, /* popcnt  6 */
            .isolated_pawn_mask           = 0x0505050505050505ull, /* popcnt 16 */
            .supported_pawn_mask          = 0x0505050505000000ull, /* popcnt 10 */
            .doubled_pawn_mask            = 0x0000000000000202ull, /* popcnt  2 */
        },
        { /* black square c3 */
            .passed_pawn_mask             = 0x0000000000000E0Eull, /* popcnt  6 */
            .isolated_pawn_mask           = 0x0A0A0A0A0A0A0A0Aull, /* popcnt 16 */
            .supported_pawn_mask          = 0x0A0A0A0A0A000000ull, /* popcnt 10 */
            .doubled_pawn_mask            = 0x0000000000000404ull, /* popcnt  2 */
        },
        { /* black square d3 */
            .passed_pawn_mask             = 0x0000000000001C1Cull, /* popcnt  6 */
            .isolated_pawn_mask           = 0x1414141414141414ull, /* popcnt 16 */
            .supported_pawn_mask          = 0x1414141414000000ull, /* popcnt 10 */
            .doubled_pawn_mask            = 0x0000000000000808ull, /* popcnt  2 */
        },
        { /* black square e3 */
            .passed_pawn_mask             = 0x0000000000003838ull, /* popcnt  6 */
            .isolated_pawn_mask           = 0x2828282828282828ull, /* popcnt 16 */
            .supported_pawn_mask          = 0x2828282828000000ull, /* popcnt 10 */
            .doubled_pawn_mask            = 0x0000000000001010ull, /* popcnt  2 */
        },
        { /* black square f3 */
            .passed_pawn_mask             = 0x0000000000007070ull, /* popcnt  6 */
            .isolated_pawn_mask           = 0x5050505050505050ull, /* popcnt 16 */
            .supported_pawn_mask          = 0x5050505050000000ull, /* popcnt 10 */
            .doubled_pawn_mask            = 0x0000000000002020ull, /* popcnt  2 */
        },
        { /* black square g3 */
            .passed_pawn_mask             = 0x000000000000E0E0ull, /* popcnt  6 */
            .isolated_pawn_mask           = 0xA0A0A0A0A0A0A0A0ull, /* popcnt 16 */
            .supported_pawn_mask          = 0xA0A0A0A0A0000000ull, /* popcnt 10 */
            .doubled_pawn_mask            = 0x0000000000004040ull, /* popcnt  2 */
        },
        { /* black square h3 */
            .passed_pawn_mask             = 0x000000000000C0C0ull, /* popcnt  4 */
            .isolated_pawn_mask           = 0x4040404040404040ull, /* popcnt  8 */
            .supported_pawn_mask          = 0x4040404040000000ull, /* popcnt  5 */
            .doubled_pawn_mask            = 0x0000000000008080ull, /* popcnt  2 */
        },
        { /* black square a4 */
            .passed_pawn_mask             = 0x0000000000030303ull, /* popcnt  6 */
            .isolated_pawn_mask           = 0x0202020202020202ull, /* popcnt  8 */
            .supported_pawn_mask          = 0x0202020200000000ull, /* popcnt  4 */
            .doubled_pawn_mask            = 0x0000000000010101ull, /* popcnt  3 */
        },
        { /* black square b4 */
            .passed_pawn_mask             = 0x0000000000070707ull, /* popcnt  9 */
            .isolated_pawn_mask           = 0x0505050505050505ull, /* popcnt 16 */
            .supported_pawn_mask          = 0x0505050500000000ull, /* popcnt  8 */
            .doubled_pawn_mask            = 0x0000000000020202ull, /* popcnt  3 */
        },
        { /* black square c4 */
            .passed_pawn_mask             = 0x00000000000E0E0Eull, /* popcnt  9 */
            .isolated_pawn_mask           = 0x0A0A0A0A0A0A0A0Aull, /* popcnt 16 */
            .supported_pawn_mask          = 0x0A0A0A0A00000000ull, /* popcnt  8 */
            .doubled_pawn_mask            = 0x0000000000040404ull, /* popcnt  3 */
        },
        { /* black square d4 */
            .passed_pawn_mask             = 0x00000000001C1C1Cull, /* popcnt  9 */
            .isolated_pawn_mask           = 0x1414141414141414ull, /* popcnt 16 */
            .supported_pawn_mask          = 0x1414141400000000ull, /* popcnt  8 */
            .doubled_pawn_mask            = 0x0000000000080808ull, /* popcnt  3 */
        },
        { /* black square e4 */
            .passed_pawn_mask             = 0x0000000000383838ull, /* popcnt  9 */
            .isolated_pawn_mask           = 0x2828282828282828ull, /* popcnt 16 */
            .supported_pawn_mask          = 0x2828282800000000ull, /* popcnt  8 */
            .doubled_pawn_mask            = 0x0000000000101010ull, /* popcnt  3 */
        },
        { /* black square f4 */
            .passed_pawn_mask             = 0x0000000000707070ull, /* popcnt  9 */
            .isolated_pawn_mask           = 0x5050505050505050ull, /* popcnt 16 */
            .supported_pawn_mask          = 0x5050505000000000ull, /* popcnt  8 */
            .doubled_pawn_mask            = 0x0000000000202020ull, /* popcnt  3 */
        },
        { /* black square g4 */
            .passed_pawn_mask             = 0x0000000000E0E0E0ull, /* popcnt  9 */
            .isolated_pawn_mask           = 0xA0A0A0A0A0A0A0A0ull, /* popcnt 16 */
            .supported_pawn_mask          = 0xA0A0A0A000000000ull, /* popcnt  8 */
            .doubled_pawn_mask            = 0x0000000000404040ull, /* popcnt  3 */
        },
        { /* black square h4 */
            .passed_pawn_mask             = 0x0000000000C0C0C0ull, /* popcnt  6 */
            .isolated_pawn_mask           = 0x4040404040404040ull, /* popcnt  8 */
            .supported_pawn_mask          = 0x4040404000000000ull, /* popcnt  4 */
            .doubled_pawn_mask            = 0x0000000000808080ull, /* popcnt  3 */
        },
        { /* black square a5 */
            .passed_pawn_mask             = 0x0000000003030303ull, /* popcnt  8 */
            .isolated_pawn_mask           = 0x0202020202020202ull, /* popcnt  8 */
            .supported_pawn_mask          = 0x0202020000000000ull, /* popcnt  3 */
            .doubled_pawn_mask            = 0x0000000001010101ull, /* popcnt  4 */
        },
        { /* black square b5 */
            .passed_pawn_mask             = 0x0000000007070707ull, /* popcnt 12 */
            .isolated_pawn_mask           = 0x0505050505050505ull, /* popcnt 16 */
            .supported_pawn_mask          = 0x0505050000000000ull, /* popcnt  6 */
            .doubled_pawn_mask            = 0x0000000002020202ull, /* popcnt  4 */
        },
        { /* black square c5 */
            .passed_pawn_mask             = 0x000000000E0E0E0Eull, /* popcnt 12 */
            .isolated_pawn_mask           = 0x0A0A0A0A0A0A0A0Aull, /* popcnt 16 */
            .supported_pawn_mask          = 0x0A0A0A0000000000ull, /* popcnt  6 */
            .doubled_pawn_mask            = 0x0000000004040404ull, /* popcnt  4 */
        },
        { /* black square d5 */
            .passed_pawn_mask             = 0x000000001C1C1C1Cull, /* popcnt 12 */
            .isolated_pawn_mask           = 0x1414141414141414ull, /* popcnt 16 */
            .supported_pawn_mask          = 0x1414140000000000ull, /* popcnt  6 */
            .doubled_pawn_mask            = 0x0000000008080808ull, /* popcnt  4 */
        },
        { /* black square e5 */
            .passed_pawn_mask             = 0x0000000038383838ull, /* popcnt 12 */
            .isolated_pawn_mask           = 0x2828282828282828ull, /* popcnt 16 */
            .supported_pawn_mask          = 0x2828280000000000ull, /* popcnt  6 */
            .doubled_pawn_mask            = 0x0000000010101010ull, /* popcnt  4 */
        },
        { /* black square f5 */
            .passed_pawn_mask             = 0x0000000070707070ull, /* popcnt 12 */
            .isolated_pawn_mask           = 0x5050505050505050ull, /* popcnt 16 */
            .supported_pawn_mask          = 0x5050500000000000ull, /* popcnt  6 */
            .doubled_pawn_mask            = 0x0000000020202020ull, /* popcnt  4 */
        },
        { /* black square g5 */
            .passed_pawn_mask             = 0x00000000E0E0E0E0ull, /* popcnt 12 */
            .isolated_pawn_mask           = 0xA0A0A0A0A0A0A0A0ull, /* popcnt 16 */
            .supported_pawn_mask          = 0xA0A0A00000000000ull, /* popcnt  6 */
            .doubled_pawn_mask            = 0x0000000040404040ull, /* popcnt  4 */
        },
        { /* black square h5 */
            .passed_pawn_mask             = 0x00000000C0C0C0C0ull, /* popcnt  8 */
            .isolated_pawn_mask           = 0x4040404040404040ull, /* popcnt  8 */
            .supported_pawn_mask          = 0x4040400000000000ull, /* popcnt  3 */
            .doubled_pawn_mask            = 0x0000000080808080ull, /* popcnt  4 */
        },
        { /* black square a6 */
            .passed_pawn_mask             = 0x0000000303030303ull, /* popcnt 10 */
            .isolated_pawn_mask           = 0x0202020202020202ull, /* popcnt  8 */
            .supported_pawn_mask          = 0x0202000000000000ull, /* popcnt  2 */
            .doubled_pawn_mask            = 0x0000000101010101ull, /* popcnt  5 */
        },
        { /* black square b6 */
            .passed_pawn_mask             = 0x0000000707070707ull, /* popcnt 15 */
            .isolated_pawn_mask           = 0x0505050505050505ull, /* popcnt 16 */
            .supported_pawn_mask          = 0x0505000000000000ull, /* popcnt  4 */
            .doubled_pawn_mask            = 0x0000000202020202ull, /* popcnt  5 */
        },
        { /* black square c6 */
            .passed_pawn_mask             = 0x0000000E0E0E0E0Eull, /* popcnt 15 */
            .isolated_pawn_mask           = 0x0A0A0A0A0A0A0A0Aull, /* popcnt 16 */
            .supported_pawn_mask          = 0x0A0A000000000000ull, /* popcnt  4 */
            .doubled_pawn_mask            = 0x0000000404040404ull, /* popcnt  5 */
        },
        { /* black square d6 */
            .passed_pawn_mask             = 0x0000001C1C1C1C1Cull, /* popcnt 15 */
            .isolated_pawn_mask           = 0x1414141414141414ull, /* popcnt 16 */
            .supported_pawn_mask          = 0x1414000000000000ull, /* popcnt  4 */
            .doubled_pawn_mask            = 0x0000000808080808ull, /* popcnt  5 */
        },
        { /* black square e6 */
            .passed_pawn_mask             = 0x0000003838383838ull, /* popcnt 15 */
            .isolated_pawn_mask           = 0x2828282828282828ull, /* popcnt 16 */
            .supported_pawn_mask          = 0x2828000000000000ull, /* popcnt  4 */
            .doubled_pawn_mask            = 0x0000001010101010ull, /* popcnt  5 */
        },
        { /* black square f6 */
            .passed_pawn_mask             = 0x0000007070707070ull, /* popcnt 15 */
            .isolated_pawn_mask           = 0x5050505050505050ull, /* popcnt 16 */
            .supported_pawn_mask          = 0x5050000000000000ull, /* popcnt  4 */
            .doubled_pawn_mask            = 0x0000002020202020ull, /* popcnt  5 */
        },
        { /* black square g6 */
            .passed_pawn_mask             = 0x000000E0E0E0E0E0ull, /* popcnt 15 */
            .isolated_pawn_mask           = 0xA0A0A0A0A0A0A0A0ull, /* popcnt 16 */
            .supported_pawn_mask          = 0xA0A0000000000000ull, /* popcnt  4 */
            .doubled_pawn_mask            = 0x0000004040404040ull, /* popcnt  5 */
        },
        { /* black square h6 */
            .passed_pawn_mask             = 0x000000C0C0C0C0C0ull, /* popcnt 10 */
            .isolated_pawn_mask           = 0x4040404040404040ull, /* popcnt  8 */
            .supported_pawn_mask          = 0x4040000000000000ull, /* popcnt  2 */
            .doubled_pawn_mask            = 0x0000008080808080ull, /* popcnt  5 */
        },
        { /* black square a7 */
            .passed_pawn_mask             = 0x0000030303030303ull, /* popcnt 12 */
            .isolated_pawn_mask           = 0x0202020202020202ull, /* popcnt  8 */
            .supported_pawn_mask          = 0x0200000000000000ull, /* popcnt  1 */
            .doubled_pawn_mask            = 0x0000010101010101ull, /* popcnt  6 */
        },
        { /* black square b7 */
            .passed_pawn_mask             = 0x0000070707070707ull, /* popcnt 18 */
            .isolated_pawn_mask           = 0x0505050505050505ull, /* popcnt 16 */
            .supported_pawn_mask          = 0x0500000000000000ull, /* popcnt  2 */
            .doubled_pawn_mask            = 0x0000020202020202ull, /* popcnt  6 */
        },
        { /* black square c7 */
            .passed_pawn_mask             = 0x00000E0E0E0E0E0Eull, /* popcnt 18 */
            .isolated_pawn_mask           = 0x0A0A0A0A0A0A0A0Aull, /* popcnt 16 */
            .supported_pawn_mask          = 0x0A00000000000000ull, /* popcnt  2 */
            .doubled_pawn_mask            = 0x0000040404040404ull, /* popcnt  6 */
        },
        { /* black square d7 */
            .passed_pawn_mask             = 0x00001C1C1C1C1C1Cull, /* popcnt 18 */
            .isolated_pawn_mask           = 0x1414141414141414ull, /* popcnt 16 */
            .supported_pawn_mask          = 0x1400000000000000ull, /* popcnt  2 */
            .doubled_pawn_mask            = 0x0000080808080808ull, /* popcnt  6 */
        },
        { /* black square e7 */
            .passed_pawn_mask             = 0x0000383838383838ull, /* popcnt 18 */
            .isolated_pawn_mask           = 0x2828282828282828ull, /* popcnt 16 */
            .supported_pawn_mask          = 0x2800000000000000ull, /* popcnt  2 */
            .doubled_pawn_mask            = 0x0000101010101010ull, /* popcnt  6 */
        },
        { /* black square f7 */
            .passed_pawn_mask             = 0x0000707070707070ull, /* popcnt 18 */
            .isolated_pawn_mask           = 0x5050505050505050ull, /* popcnt 16 */
            .supported_pawn_mask          = 0x5000000000000000ull, /* popcnt  2 */
            .doubled_pawn_mask            = 0x0000202020202020ull, /* popcnt  6 */
        },
        { /* black square g7 */
            .passed_pawn_mask             = 0x0000E0E0E0E0E0E0ull, /* popcnt 18 */
            .isolated_pawn_mask           = 0xA0A0A0A0A0A0A0A0ull, /* popcnt 16 */
            .supported_pawn_mask          = 0xA000000000000000ull, /* popcnt  2 */
            .doubled_pawn_mask            = 0x0000404040404040ull, /* popcnt  6 */
        },
        { /* black square h7 */
            .passed_pawn_mask             = 0x0000C0C0C0C0C0C0ull, /* popcnt 12 */
            .isolated_pawn_mask           = 0x4040404040404040ull, /* popcnt  8 */
            .supported_pawn_mask          = 0x4000000000000000ull, /* popcnt  1 */
            .doubled_pawn_mask            = 0x0000808080808080ull, /* popcnt  6 */
        },
        { /* black square a8 */
            .passed_pawn_mask             = 0x0003030303030303ull, /* popcnt 14 */
            .isolated_pawn_mask           = 0x0202020202020202ull, /* popcnt  8 */
            .supported_pawn_mask          = 0x0000000000000000ull, /* popcnt  0 */
            .doubled_pawn_mask            = 0x0001010101010101ull, /* popcnt  7 */
        },
        { /* black square b8 */
            .passed_pawn_mask             = 0x0007070707070707ull, /* popcnt 21 */
            .isolated_pawn_mask           = 0x0505050505050505ull, /* popcnt 16 */
            .supported_pawn_mask          = 0x0000000000000000ull, /* popcnt  0 */
            .doubled_pawn_mask            = 0x0002020202020202ull, /* popcnt  7 */
        },
        { /* black square c8 */
            .passed_pawn_mask             = 0x000E0E0E0E0E0E0Eull, /* popcnt 21 */
            .isolated_pawn_mask           = 0x0A0A0A0A0A0A0A0Aull, /* popcnt 16 */
            .supported_pawn_mask          = 0x0000000000000000ull, /* popcnt  0 */
            .doubled_pawn_mask            = 0x0004040404040404ull, /* popcnt  7 */
        },
        { /* black square d8 */
            .passed_pawn_mask             = 0x001C1C1C1C1C1C1Cull, /* popcnt 21 */
            .isolated_pawn_mask           = 0x1414141414141414ull, /* popcnt 16 */
            .supported_pawn_mask          = 0x0000000000000000ull, /* popcnt  0 */
            .doubled_pawn_mask            = 0x0008080808080808ull, /* popcnt  7 */
        },
        { /* black square e8 */
            .passed_pawn_mask             = 0x0038383838383838ull, /* popcnt 21 */
            .isolated_pawn_mask           = 0x2828282828282828ull, /* popcnt 16 */
            .supported_pawn_mask          = 0x0000000000000000ull, /* popcnt  0 */
            .doubled_pawn_mask            = 0x0010101010101010ull, /* popcnt  7 */
        },
        { /* black square f8 */
            .passed_pawn_mask             = 0x0070707070707070ull, /* popcnt 21 */
            .isolated_pawn_mask           = 0x5050505050505050ull, /* popcnt 16 */
            .supported_pawn_mask          = 0x0000000000000000ull, /* popcnt  0 */
            .doubled_pawn_mask            = 0x0020202020202020ull, /* popcnt  7 */
        },
        { /* black square g8 */
            .passed_pawn_mask             = 0x00E0E0E0E0E0E0E0ull, /* popcnt 21 */
            .isolated_pawn_mask           = 0xA0A0A0A0A0A0A0A0ull, /* popcnt 16 */
            .supported_pawn_mask          = 0x0000000000000000ull, /* popcnt  0 */
            .doubled_pawn_mask            = 0x0040404040404040ull, /* popcnt  7 */
        },
        { /* black square h8 */
            .passed_pawn_mask             = 0x00C0C0C0C0C0C0C0ull, /* popcnt 14 */
            .isolated_pawn_mask           = 0x4040404040404040ull, /* popcnt  8 */
            .supported_pawn_mask          = 0x0000000000000000ull, /* popcnt  0 */
            .doubled_pawn_mask            = 0x0080808080808080ull, /* popcnt  7 */
        },
    },
};
const bitboard INTERVENING_SQUARES[64][64] = 
{
    { /* square a1 */
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000002ull,0x0000000000000006ull,
        0x000000000000000Eull,0x000000000000001Eull,0x000000000000003Eull,0x000000000000007Eull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000100ull,0x0000000000000000ull,0x0000000000000200ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000010100ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000040200ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000001010100ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000008040200ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000101010100ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000001008040200ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000010101010100ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000201008040200ull,0x0000000000000000ull,
        0x0001010101010100ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0040201008040200ull,
    },
    { /* square b1 */
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000004ull,
        0x000000000000000Cull,0x000000000000001Cull,0x000000000000003Cull,0x000000000000007Cull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000200ull,0x0000000000000000ull,0x0000000000000400ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000020200ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000080400ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000002020200ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000010080400ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000202020200ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000002010080400ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000020202020200ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000402010080400ull,
        0x0000000000000000ull,0x0002020202020200ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
    },
    { /* square c1 */
        0x0000000000000002ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000008ull,0x0000000000000018ull,0x0000000000000038ull,0x0000000000000078ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000200ull,0x0000000000000000ull,0x0000000000000400ull,0x0000000000000000ull,
        0x0000000000000800ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000040400ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000100800ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000004040400ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000020100800ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000404040400ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000004020100800ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000040404040400ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0004040404040400ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
    },
    { /* square d1 */
        0x0000000000000006ull,0x0000000000000004ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000010ull,0x0000000000000030ull,0x0000000000000070ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000400ull,0x0000000000000000ull,0x0000000000000800ull,
        0x0000000000000000ull,0x0000000000001000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000020400ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000080800ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000201000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000008080800ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000040201000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000808080800ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000080808080800ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0008080808080800ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
    },
    { /* square e1 */
        0x000000000000000Eull,0x000000000000000Cull,0x0000000000000008ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000020ull,0x0000000000000060ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000800ull,0x0000000000000000ull,
        0x0000000000001000ull,0x0000000000000000ull,0x0000000000002000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000040800ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000101000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000402000ull,
        0x0000000002040800ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000010101000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000001010101000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000101010101000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0010101010101000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
    },
    { /* square f1 */
        0x000000000000001Eull,0x000000000000001Cull,0x0000000000000018ull,0x0000000000000010ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000040ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000001000ull,
        0x0000000000000000ull,0x0000000000002000ull,0x0000000000000000ull,0x0000000000004000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000081000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000202000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000004081000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000020202000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000204081000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000002020202000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000202020202000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0020202020202000ull,0x0000000000000000ull,0x0000000000000000ull,
    },
    { /* square g1 */
        0x000000000000003Eull,0x000000000000003Cull,0x0000000000000038ull,0x0000000000000030ull,
        0x0000000000000020ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000002000ull,0x0000000000000000ull,0x0000000000004000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000102000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000404000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000008102000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000040404000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000408102000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000004040404000ull,0x0000000000000000ull,
        0x0000020408102000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000404040404000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0040404040404000ull,0x0000000000000000ull,
    },
    { /* square h1 */
        0x000000000000007Eull,0x000000000000007Cull,0x0000000000000078ull,0x0000000000000070ull,
        0x0000000000000060ull,0x0000000000000040ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000004000ull,0x0000000000000000ull,0x0000000000008000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000204000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000808000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000010204000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000080808000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000810204000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000008080808000ull,
        0x0000000000000000ull,0x0000040810204000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000808080808000ull,
        0x0002040810204000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0080808080808000ull,
    },
    { /* square a2 */
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000200ull,0x0000000000000600ull,
        0x0000000000000E00ull,0x0000000000001E00ull,0x0000000000003E00ull,0x0000000000007E00ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000010000ull,0x0000000000000000ull,0x0000000000020000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000001010000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000004020000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000101010000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000804020000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000010101010000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000100804020000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0001010101010000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0020100804020000ull,0x0000000000000000ull,
    },
    { /* square b2 */
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000400ull,
        0x0000000000000C00ull,0x0000000000001C00ull,0x0000000000003C00ull,0x0000000000007C00ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000020000ull,0x0000000000000000ull,0x0000000000040000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000002020000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000008040000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000202020000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000001008040000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000020202020000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000201008040000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0002020202020000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0040201008040000ull,
    },
    { /* square c2 */
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000200ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000800ull,0x0000000000001800ull,0x0000000000003800ull,0x0000000000007800ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000020000ull,0x0000000000000000ull,0x0000000000040000ull,0x0000000000000000ull,
        0x0000000000080000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000004040000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000010080000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000404040000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000002010080000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000040404040000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000402010080000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0004040404040000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
    },
    { /* square d2 */
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000600ull,0x0000000000000400ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000001000ull,0x0000000000003000ull,0x0000000000007000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000040000ull,0x0000000000000000ull,0x0000000000080000ull,
        0x0000000000000000ull,0x0000000000100000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000002040000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000008080000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000020100000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000808080000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000004020100000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000080808080000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0008080808080000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
    },
    { /* square e2 */
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000E00ull,0x0000000000000C00ull,0x0000000000000800ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000002000ull,0x0000000000006000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000080000ull,0x0000000000000000ull,
        0x0000000000100000ull,0x0000000000000000ull,0x0000000000200000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000004080000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000010100000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000040200000ull,
        0x0000000204080000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000001010100000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000101010100000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0010101010100000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
    },
    { /* square f2 */
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000001E00ull,0x0000000000001C00ull,0x0000000000001800ull,0x0000000000001000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000004000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000100000ull,
        0x0000000000000000ull,0x0000000000200000ull,0x0000000000000000ull,0x0000000000400000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000008100000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000020200000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000408100000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000002020200000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000020408100000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000202020200000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0020202020200000ull,0x0000000000000000ull,0x0000000000000000ull,
    },
    { /* square g2 */
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000003E00ull,0x0000000000003C00ull,0x0000000000003800ull,0x0000000000003000ull,
        0x0000000000002000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000200000ull,0x0000000000000000ull,0x0000000000400000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000010200000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000040400000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000810200000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000004040400000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000040810200000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000404040400000ull,0x0000000000000000ull,
        0x0002040810200000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0040404040400000ull,0x0000000000000000ull,
    },
    { /* square h2 */
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000007E00ull,0x0000000000007C00ull,0x0000000000007800ull,0x0000000000007000ull,
        0x0000000000006000ull,0x0000000000004000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000400000ull,0x0000000000000000ull,0x0000000000800000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000020400000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000080800000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000001020400000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000008080800000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000081020400000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000808080800000ull,
        0x0000000000000000ull,0x0004081020400000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0080808080800000ull,
    },
    { /* square a3 */
        0x0000000000000100ull,0x0000000000000000ull,0x0000000000000200ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000020000ull,0x0000000000060000ull,
        0x00000000000E0000ull,0x00000000001E0000ull,0x00000000003E0000ull,0x00000000007E0000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000001000000ull,0x0000000000000000ull,0x0000000002000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000101000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000402000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000010101000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000080402000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0001010101000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0010080402000000ull,0x0000000000000000ull,0x0000000000000000ull,
    },
    { /* square b3 */
        0x0000000000000000ull,0x0000000000000200ull,0x0000000000000000ull,0x0000000000000400ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000040000ull,
        0x00000000000C0000ull,0x00000000001C0000ull,0x00000000003C0000ull,0x00000000007C0000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000002000000ull,0x0000000000000000ull,0x0000000004000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000202000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000804000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000020202000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000100804000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0002020202000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0020100804000000ull,0x0000000000000000ull,
    },
    { /* square c3 */
        0x0000000000000200ull,0x0000000000000000ull,0x0000000000000400ull,0x0000000000000000ull,
        0x0000000000000800ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000020000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000080000ull,0x0000000000180000ull,0x0000000000380000ull,0x0000000000780000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000002000000ull,0x0000000000000000ull,0x0000000004000000ull,0x0000000000000000ull,
        0x0000000008000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000404000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000001008000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000040404000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000201008000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0004040404000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0040201008000000ull,
    },
    { /* square d3 */
        0x0000000000000000ull,0x0000000000000400ull,0x0000000000000000ull,0x0000000000000800ull,
        0x0000000000000000ull,0x0000000000001000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000060000ull,0x0000000000040000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000100000ull,0x0000000000300000ull,0x0000000000700000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000004000000ull,0x0000000000000000ull,0x0000000008000000ull,
        0x0000000000000000ull,0x0000000010000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000204000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000808000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000002010000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000080808000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000402010000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0008080808000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
    },
    { /* square e3 */
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000800ull,0x0000000000000000ull,
        0x0000000000001000ull,0x0000000000000000ull,0x0000000000002000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x00000000000E0000ull,0x00000000000C0000ull,0x0000000000080000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000200000ull,0x0000000000600000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000008000000ull,0x0000000000000000ull,
        0x0000000010000000ull,0x0000000000000000ull,0x0000000020000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000408000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000001010000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000004020000000ull,
        0x0000020408000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000101010000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0010101010000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
    },
    { /* square f3 */
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000001000ull,
        0x0000000000000000ull,0x0000000000002000ull,0x0000000000000000ull,0x0000000000004000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x00000000001E0000ull,0x00000000001C0000ull,0x0000000000180000ull,0x0000000000100000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000400000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000010000000ull,
        0x0000000000000000ull,0x0000000020000000ull,0x0000000000000000ull,0x0000000040000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000810000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000002020000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000040810000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000202020000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0002040810000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0020202020000000ull,0x0000000000000000ull,0x0000000000000000ull,
    },
    { /* square g3 */
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000002000ull,0x0000000000000000ull,0x0000000000004000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x00000000003E0000ull,0x00000000003C0000ull,0x0000000000380000ull,0x0000000000300000ull,
        0x0000000000200000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000020000000ull,0x0000000000000000ull,0x0000000040000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000001020000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000004040000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000081020000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000404040000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0004081020000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0040404040000000ull,0x0000000000000000ull,
    },
    { /* square h3 */
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000004000ull,0x0000000000000000ull,0x0000000000008000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x00000000007E0000ull,0x00000000007C0000ull,0x0000000000780000ull,0x0000000000700000ull,
        0x0000000000600000ull,0x0000000000400000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000040000000ull,0x0000000000000000ull,0x0000000080000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000002040000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000008080000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000102040000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000808080000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0008102040000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0080808080000000ull,
    },
    { /* square a4 */
        0x0000000000010100ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000020400ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000010000ull,0x0000000000000000ull,0x0000000000020000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000002000000ull,0x0000000006000000ull,
        0x000000000E000000ull,0x000000001E000000ull,0x000000003E000000ull,0x000000007E000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000100000000ull,0x0000000000000000ull,0x0000000200000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000010100000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000040200000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0001010100000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0008040200000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
    },
    { /* square b4 */
        0x0000000000000000ull,0x0000000000020200ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000040800ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000020000ull,0x0000000000000000ull,0x0000000000040000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000004000000ull,
        0x000000000C000000ull,0x000000001C000000ull,0x000000003C000000ull,0x000000007C000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000200000000ull,0x0000000000000000ull,0x0000000400000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000020200000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000080400000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0002020200000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0010080400000000ull,0x0000000000000000ull,0x0000000000000000ull,
    },
    { /* square c4 */
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000040400ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000081000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000020000ull,0x0000000000000000ull,0x0000000000040000ull,0x0000000000000000ull,
        0x0000000000080000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000002000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000008000000ull,0x0000000018000000ull,0x0000000038000000ull,0x0000000078000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000200000000ull,0x0000000000000000ull,0x0000000400000000ull,0x0000000000000000ull,
        0x0000000800000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000040400000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000100800000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0004040400000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0020100800000000ull,0x0000000000000000ull,
    },
    { /* square d4 */
        0x0000000000040200ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000080800ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000102000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000040000ull,0x0000000000000000ull,0x0000000000080000ull,
        0x0000000000000000ull,0x0000000000100000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000006000000ull,0x0000000004000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000010000000ull,0x0000000030000000ull,0x0000000070000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000400000000ull,0x0000000000000000ull,0x0000000800000000ull,
        0x0000000000000000ull,0x0000001000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000020400000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000080800000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000201000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0008080800000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0040201000000000ull,
    },
    { /* square e4 */
        0x0000000000000000ull,0x0000000000080400ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000101000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000204000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000080000ull,0x0000000000000000ull,
        0x0000000000100000ull,0x0000000000000000ull,0x0000000000200000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x000000000E000000ull,0x000000000C000000ull,0x0000000008000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000020000000ull,0x0000000060000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000800000000ull,0x0000000000000000ull,
        0x0000001000000000ull,0x0000000000000000ull,0x0000002000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000040800000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000101000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000402000000000ull,
        0x0002040800000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0010101000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
    },
    { /* square f4 */
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000100800ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000202000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000100000ull,
        0x0000000000000000ull,0x0000000000200000ull,0x0000000000000000ull,0x0000000000400000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x000000001E000000ull,0x000000001C000000ull,0x0000000018000000ull,0x0000000010000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000040000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000001000000000ull,
        0x0000000000000000ull,0x0000002000000000ull,0x0000000000000000ull,0x0000004000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000081000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000202000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0004081000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0020202000000000ull,0x0000000000000000ull,0x0000000000000000ull,
    },
    { /* square g4 */
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000201000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000404000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000200000ull,0x0000000000000000ull,0x0000000000400000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x000000003E000000ull,0x000000003C000000ull,0x0000000038000000ull,0x0000000030000000ull,
        0x0000000020000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000002000000000ull,0x0000000000000000ull,0x0000004000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000102000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000404000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0008102000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0040404000000000ull,0x0000000000000000ull,
    },
    { /* square h4 */
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000402000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000808000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000400000ull,0x0000000000000000ull,0x0000000000800000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x000000007E000000ull,0x000000007C000000ull,0x0000000078000000ull,0x0000000070000000ull,
        0x0000000060000000ull,0x0000000040000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000004000000000ull,0x0000000000000000ull,0x0000008000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000204000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000808000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0010204000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0080808000000000ull,
    },
    { /* square a5 */
        0x0000000001010100ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000002040800ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000001010000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000002040000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000001000000ull,0x0000000000000000ull,0x0000000002000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000200000000ull,0x0000000600000000ull,
        0x0000000E00000000ull,0x0000001E00000000ull,0x0000003E00000000ull,0x0000007E00000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000010000000000ull,0x0000000000000000ull,0x0000020000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0001010000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0004020000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
    },
    { /* square b5 */
        0x0000000000000000ull,0x0000000002020200ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000004081000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000002020000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000004080000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000002000000ull,0x0000000000000000ull,0x0000000004000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000400000000ull,
        0x0000000C00000000ull,0x0000001C00000000ull,0x0000003C00000000ull,0x0000007C00000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000020000000000ull,0x0000000000000000ull,0x0000040000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0002020000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0008040000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
    },
    { /* square c5 */
        0x0000000000000000ull,0x0000000000000000ull,0x0000000004040400ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000008102000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000004040000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000008100000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000002000000ull,0x0000000000000000ull,0x0000000004000000ull,0x0000000000000000ull,
        0x0000000008000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000200000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000800000000ull,0x0000001800000000ull,0x0000003800000000ull,0x0000007800000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000020000000000ull,0x0000000000000000ull,0x0000040000000000ull,0x0000000000000000ull,
        0x0000080000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0004040000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0010080000000000ull,0x0000000000000000ull,0x0000000000000000ull,
    },
    { /* square d5 */
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000008080800ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000010204000ull,
        0x0000000004020000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000008080000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000010200000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000004000000ull,0x0000000000000000ull,0x0000000008000000ull,
        0x0000000000000000ull,0x0000000010000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000600000000ull,0x0000000400000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000001000000000ull,0x0000003000000000ull,0x0000007000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000040000000000ull,0x0000000000000000ull,0x0000080000000000ull,
        0x0000000000000000ull,0x0000100000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0002040000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0008080000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0020100000000000ull,0x0000000000000000ull,
    },
    { /* square e5 */
        0x0000000008040200ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000010101000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000008040000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000010100000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000020400000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000008000000ull,0x0000000000000000ull,
        0x0000000010000000ull,0x0000000000000000ull,0x0000000020000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000E00000000ull,0x0000000C00000000ull,0x0000000800000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000002000000000ull,0x0000006000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000080000000000ull,0x0000000000000000ull,
        0x0000100000000000ull,0x0000000000000000ull,0x0000200000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0004080000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0010100000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0040200000000000ull,
    },
    { /* square f5 */
        0x0000000000000000ull,0x0000000010080400ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000020202000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000010080000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000020200000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000010000000ull,
        0x0000000000000000ull,0x0000000020000000ull,0x0000000000000000ull,0x0000000040000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000001E00000000ull,0x0000001C00000000ull,0x0000001800000000ull,0x0000001000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000004000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000100000000000ull,
        0x0000000000000000ull,0x0000200000000000ull,0x0000000000000000ull,0x0000400000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0008100000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0020200000000000ull,0x0000000000000000ull,0x0000000000000000ull,
    },
    { /* square g5 */
        0x0000000000000000ull,0x0000000000000000ull,0x0000000020100800ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000040404000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000020100000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000040400000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000020000000ull,0x0000000000000000ull,0x0000000040000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000003E00000000ull,0x0000003C00000000ull,0x0000003800000000ull,0x0000003000000000ull,
        0x0000002000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000200000000000ull,0x0000000000000000ull,0x0000400000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0010200000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0040400000000000ull,0x0000000000000000ull,
    },
    { /* square h5 */
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000040201000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000080808000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000040200000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000080800000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000040000000ull,0x0000000000000000ull,0x0000000080000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000007E00000000ull,0x0000007C00000000ull,0x0000007800000000ull,0x0000007000000000ull,
        0x0000006000000000ull,0x0000004000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000400000000000ull,0x0000000000000000ull,0x0000800000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0020400000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0080800000000000ull,
    },
    { /* square a6 */
        0x0000000101010100ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000204081000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000101010000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000204080000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000101000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000204000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000100000000ull,0x0000000000000000ull,0x0000000200000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000020000000000ull,0x0000060000000000ull,
        0x00000E0000000000ull,0x00001E0000000000ull,0x00003E0000000000ull,0x00007E0000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0001000000000000ull,0x0000000000000000ull,0x0002000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
    },
    { /* square b6 */
        0x0000000000000000ull,0x0000000202020200ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000408102000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000202020000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000408100000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000202000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000408000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000200000000ull,0x0000000000000000ull,0x0000000400000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000040000000000ull,
        0x00000C0000000000ull,0x00001C0000000000ull,0x00003C0000000000ull,0x00007C0000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0002000000000000ull,0x0000000000000000ull,0x0004000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
    },
    { /* square c6 */
        0x0000000000000000ull,0x0000000000000000ull,0x0000000404040400ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000810204000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000404040000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000810200000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000404000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000810000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000200000000ull,0x0000000000000000ull,0x0000000400000000ull,0x0000000000000000ull,
        0x0000000800000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000020000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000080000000000ull,0x0000180000000000ull,0x0000380000000000ull,0x0000780000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0002000000000000ull,0x0000000000000000ull,0x0004000000000000ull,0x0000000000000000ull,
        0x0008000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
    },
    { /* square d6 */
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000808080800ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000808080000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000001020400000ull,
        0x0000000402000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000808000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000001020000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000400000000ull,0x0000000000000000ull,0x0000000800000000ull,
        0x0000000000000000ull,0x0000001000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000060000000000ull,0x0000040000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000100000000000ull,0x0000300000000000ull,0x0000700000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0004000000000000ull,0x0000000000000000ull,0x0008000000000000ull,
        0x0000000000000000ull,0x0010000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
    },
    { /* square e6 */
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000001010101000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000804020000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000001010100000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000804000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000001010000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000002040000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000800000000ull,0x0000000000000000ull,
        0x0000001000000000ull,0x0000000000000000ull,0x0000002000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x00000E0000000000ull,0x00000C0000000000ull,0x0000080000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000200000000000ull,0x0000600000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0008000000000000ull,0x0000000000000000ull,
        0x0010000000000000ull,0x0000000000000000ull,0x0020000000000000ull,0x0000000000000000ull,
    },
    { /* square f6 */
        0x0000001008040200ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000002020202000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000001008040000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000002020200000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000001008000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000002020000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000001000000000ull,
        0x0000000000000000ull,0x0000002000000000ull,0x0000000000000000ull,0x0000004000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x00001E0000000000ull,0x00001C0000000000ull,0x0000180000000000ull,0x0000100000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000400000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0010000000000000ull,
        0x0000000000000000ull,0x0020000000000000ull,0x0000000000000000ull,0x0040000000000000ull,
    },
    { /* square g6 */
        0x0000000000000000ull,0x0000002010080400ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000004040404000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000002010080000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000004040400000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000002010000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000004040000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000002000000000ull,0x0000000000000000ull,0x0000004000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x00003E0000000000ull,0x00003C0000000000ull,0x0000380000000000ull,0x0000300000000000ull,
        0x0000200000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0020000000000000ull,0x0000000000000000ull,0x0040000000000000ull,0x0000000000000000ull,
    },
    { /* square h6 */
        0x0000000000000000ull,0x0000000000000000ull,0x0000004020100800ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000008080808000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000004020100000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000008080800000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000004020000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000008080000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000004000000000ull,0x0000000000000000ull,0x0000008000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x00007E0000000000ull,0x00007C0000000000ull,0x0000780000000000ull,0x0000700000000000ull,
        0x0000600000000000ull,0x0000400000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0040000000000000ull,0x0000000000000000ull,0x0080000000000000ull,
    },
    { /* square a7 */
        0x0000010101010100ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000020408102000ull,0x0000000000000000ull,
        0x0000010101010000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000020408100000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000010101000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000020408000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000010100000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000020400000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000010000000000ull,0x0000000000000000ull,0x0000020000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0002000000000000ull,0x0006000000000000ull,
        0x000E000000000000ull,0x001E000000000000ull,0x003E000000000000ull,0x007E000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
    },
    { /* square b7 */
        0x0000000000000000ull,0x0000020202020200ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000040810204000ull,
        0x0000000000000000ull,0x0000020202020000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000040810200000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000020202000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000040810000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000020200000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000040800000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000020000000000ull,0x0000000000000000ull,0x0000040000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0004000000000000ull,
        0x000C000000000000ull,0x001C000000000000ull,0x003C000000000000ull,0x007C000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
    },
    { /* square c7 */
        0x0000000000000000ull,0x0000000000000000ull,0x0000040404040400ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000040404040000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000081020400000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000040404000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000081020000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000040400000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000081000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000020000000000ull,0x0000000000000000ull,0x0000040000000000ull,0x0000000000000000ull,
        0x0000080000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0002000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0008000000000000ull,0x0018000000000000ull,0x0038000000000000ull,0x0078000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
    },
    { /* square d7 */
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000080808080800ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000080808080000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000080808000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000102040000000ull,
        0x0000040200000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000080800000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000102000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000040000000000ull,0x0000000000000000ull,0x0000080000000000ull,
        0x0000000000000000ull,0x0000100000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0006000000000000ull,0x0004000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0010000000000000ull,0x0030000000000000ull,0x0070000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
    },
    { /* square e7 */
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000101010101000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000101010100000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000080402000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000101010000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000080400000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000101000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000204000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000080000000000ull,0x0000000000000000ull,
        0x0000100000000000ull,0x0000000000000000ull,0x0000200000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x000E000000000000ull,0x000C000000000000ull,0x0008000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0020000000000000ull,0x0060000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
    },
    { /* square f7 */
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000202020202000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000100804020000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000202020200000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000100804000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000202020000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000100800000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000202000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000100000000000ull,
        0x0000000000000000ull,0x0000200000000000ull,0x0000000000000000ull,0x0000400000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x001E000000000000ull,0x001C000000000000ull,0x0018000000000000ull,0x0010000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0040000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
    },
    { /* square g7 */
        0x0000201008040200ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000404040404000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000201008040000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000404040400000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000201008000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000404040000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000201000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000404000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000200000000000ull,0x0000000000000000ull,0x0000400000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x003E000000000000ull,0x003C000000000000ull,0x0038000000000000ull,0x0030000000000000ull,
        0x0020000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
    },
    { /* square h7 */
        0x0000000000000000ull,0x0000402010080400ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000808080808000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000402010080000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000808080800000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000402010000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000808080000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000402000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000808000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000400000000000ull,0x0000000000000000ull,0x0000800000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x007E000000000000ull,0x007C000000000000ull,0x0078000000000000ull,0x0070000000000000ull,
        0x0060000000000000ull,0x0040000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
    },
    { /* square a8 */
        0x0001010101010100ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0002040810204000ull,
        0x0001010101010000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0002040810200000ull,0x0000000000000000ull,
        0x0001010101000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0002040810000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0001010100000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0002040800000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0001010000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0002040000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0001000000000000ull,0x0000000000000000ull,0x0002000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0200000000000000ull,0x0600000000000000ull,
        0x0E00000000000000ull,0x1E00000000000000ull,0x3E00000000000000ull,0x7E00000000000000ull,
    },
    { /* square b8 */
        0x0000000000000000ull,0x0002020202020200ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0002020202020000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0004081020400000ull,
        0x0000000000000000ull,0x0002020202000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0004081020000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0002020200000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0004081000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0002020000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0004080000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0002000000000000ull,0x0000000000000000ull,0x0004000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0400000000000000ull,
        0x0C00000000000000ull,0x1C00000000000000ull,0x3C00000000000000ull,0x7C00000000000000ull,
    },
    { /* square c8 */
        0x0000000000000000ull,0x0000000000000000ull,0x0004040404040400ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0004040404040000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0004040404000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0008102040000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0004040400000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0008102000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0004040000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0008100000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0002000000000000ull,0x0000000000000000ull,0x0004000000000000ull,0x0000000000000000ull,
        0x0008000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0200000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0800000000000000ull,0x1800000000000000ull,0x3800000000000000ull,0x7800000000000000ull,
    },
    { /* square d8 */
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0008080808080800ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0008080808080000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0008080808000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0008080800000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0010204000000000ull,
        0x0004020000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0008080000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0010200000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0004000000000000ull,0x0000000000000000ull,0x0008000000000000ull,
        0x0000000000000000ull,0x0010000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0600000000000000ull,0x0400000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x1000000000000000ull,0x3000000000000000ull,0x7000000000000000ull,
    },
    { /* square e8 */
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0010101010101000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0010101010100000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0010101010000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0008040200000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0010101000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0008040000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0010100000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0020400000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0008000000000000ull,0x0000000000000000ull,
        0x0010000000000000ull,0x0000000000000000ull,0x0020000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0E00000000000000ull,0x0C00000000000000ull,0x0800000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x2000000000000000ull,0x6000000000000000ull,
    },
    { /* square f8 */
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0020202020202000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0020202020200000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0010080402000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0020202020000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0010080400000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0020202000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0010080000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0020200000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0010000000000000ull,
        0x0000000000000000ull,0x0020000000000000ull,0x0000000000000000ull,0x0040000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x1E00000000000000ull,0x1C00000000000000ull,0x1800000000000000ull,0x1000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x4000000000000000ull,
    },
    { /* square g8 */
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0040404040404000ull,0x0000000000000000ull,
        0x0020100804020000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0040404040400000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0020100804000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0040404040000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0020100800000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0040404000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0020100000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0040400000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0020000000000000ull,0x0000000000000000ull,0x0040000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x3E00000000000000ull,0x3C00000000000000ull,0x3800000000000000ull,0x3000000000000000ull,
        0x2000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
    },
    { /* square h8 */
        0x0040201008040200ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0080808080808000ull,
        0x0000000000000000ull,0x0040201008040000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0080808080800000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0040201008000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0080808080000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0040201000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0080808000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0040200000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0080800000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0040000000000000ull,0x0000000000000000ull,0x0080000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
        0x7E00000000000000ull,0x7C00000000000000ull,0x7800000000000000ull,0x7000000000000000ull,
        0x6000000000000000ull,0x4000000000000000ull,0x0000000000000000ull,0x0000000000000000ull,
    },
};
const uint64_t PIECE_SQUARE_HASHES[2][6][64] = 
{
    {
        {   /* white pawn */
            0xBEDAF65DAD5DB60Bull,0x9706F44D0A1FE02Dull,0x481E64CCFDF3C3D8ull,0x7851343120B83DCCull,
            0x570A46177AA278FEull,0xE8E9C73BD38DEAB3ull,0x1166CA4D579B01B7ull,0x99505F621BE7F175ull,
            0xF5D67549EB6D89BBull,0x681AB9D7BA59E109ull,0x74961E9362D969A4ull,0xEC286AFA0E87D5A8ull,
            0x1BF8FFEBB803B3E8ull,0x03033E63B42A39CCull,0x44B38F52FEB12915ull,0x65CC6C2D77052597ull,
            0xB074A29B984CADF4ull,0x19D27EAF6950716Aull,0x3F3A906D478FFD11ull,0xD522D8953010D34Dull,
            0x594069F36AB05470ull,0x39CAA9522EE1515Dull,0xE348221990D49BA9ull,0x0CB732BA3893C473ull,
            0x1141CC904B09F5AAull,0xA04F22B1B77D5DE0ull,0xCED9D7FFE10FC532ull,0x70CCB666FC293894ull,
            0xD0D58D526A53C92Eull,0x7BBE2CDF46DD7193ull,0xCF43FF521AE498CFull,0x397735B98F854403ull,
            0x572E9FC9D6C7F980ull,0xFED6C01312115416ull,0x572A4682AC55480Full,0x3F8A118FEEB15BDDull,
            0xF4D5B4CDC902B8F9ull,0xE1DCD8621C67C625ull,0xAFEA07715F39F5D4ull,0x42E7BDA21E0675D4ull,
            0xBC2EF7997FDA235Cull,0x3BA40B35E08CE149ull,0xC3705BF315DBC8E4ull,0x4EEE9A375649C9E9ull,
            0xFCC696095CB284E2ull,0x420CDBD10C1FD690ull,0xA8903E10DE6884E9ull,0xD1F9EBBE51D7DCA5ull,
            0x57191D6AA4A73283ull,0x229BC804461159C1ull,0x2916F6AD72A8D270ull,0x7960836F4B11D7DBull,
            0xA5DF543961374933ull,0x10E1E68B23C2066Bull,0x8A92D09A8691786Eull,0xC74AA14F0285D107ull,
            0x6FBD1663B74E2C12ull,0xACFB33874A1D746Bull,0x97A73F7264D6A44Bull,0x12651FC3098005F3ull,
            0xAE7E40151BC6CE65ull,0x5E41AC47F12FA1C1ull,0x860B5EB09F805D1Bull,0x5EA88E7A5D13EE43ull,
        },
        {   /* white knight */
            0x04076D1787CA92BEull,0x0629E166527C3209ull,0xE075C81CE020918Eull,0x74A78C49A4A75E25ull,
            0xC369B36ADD04B147ull,0x06C5EE9A897D2DD6ull,0x44C48319872D9F64ull,0xF173B6A63716C481ull,
            0x859C6A5131B74B5Eull,0x6FEE95DB63B82D19ull,0x3755D21E9B324DE7ull,0x3888519361F015CBull,
            0x37813D30B3FCF467ull,0x48C9E3A6C0F185FAull,0xF2E2879AEC2EC3E8ull,0xC2653727076C290Bull,
            0x399BE906E1E22152ull,0x52DE906E10CD2F70ull,0x05176AB0E3C73FBDull,0x02F89EC032CAB841ull,
            0x9DD3AB72389E23E5ull,0x695D165EC37AD6DAull,0x349F4ECB79FD7B96ull,0x40265C7D82CF7242ull,
            0xC29B127809E90821ull,0x764782C4B2F9F8B5ull,0x1E566A7EF75EF9B4ull,0x8007E30CC7FB497Full,
            0x40E58825AB2CED5Full,0x6FD879EA919F92BAull,0x6D426DBF6D5D96C5ull,0x45A64B92FC2DDFC0ull,
            0xFDE9BC6ECC82E8FCull,0x5F29BBDF8FDAE79Dull,0x5150661140000310ull,0x7F6F9E8BDCC08A43ull,
            0xCA3698377D0032E4ull,0x84959AF34FD524D9ull,0x4502C5E2399EB1DFull,0xA709D248A8B8FE84ull,
            0x1AF451BC93C5733Aull,0x5310F0EEEBFC5FEBull,0xAE71D0A33543E295ull,0xA20010F076B2B25Dull,
            0xB02694621570E519ull,0x98FBA071A1B3BE00ull,0x6D72F25BCE077E0Cull,0x9C2543FD57A41696ull,
            0xC308F723DC67639Bull,0x6B4481E588206EECull,0xD0CF93EEAA1B48AEull,0x32A811E4BCA2EDE0ull,
            0xAC311AB64352252Aull,0x3D93F5C47136FF17ull,0x34762DABFDA6F041ull,0xEEBBF0A819D57E14ull,
            0xBFBEE82F92E0A606ull,0x7D1B7C95A58F407Full,0x5A6FD5C4DE662386ull,0xBBE438CE7D3DF1CFull,
            0x45954166CAA08BDFull,0x6EBB9BC66E642485ull,0x13367E5A13A25F8Bull,0x9BF287E80C57E5A1ull,
        },
        {   /* white bishop */
            0x8B01EA84E27D7697ull,0x618A38AF26DF2F75ull,0xCCBE9F1B27F82AD5ull,0xEA0900B062C26E92ull,
            0x996833DBC08F4803ull,0xF091FA2248F4DCEDull,0x7B4A47A4DA63C0FCull,0xC0725BC5F4B739C1ull,
            0x6FAB0F4A1B7121D4ull,0x344A8ADD4B676497ull,0x7C35C673C7C21F01ull,0xA28E91CEAE61BF37ull,
            0x3E4A328BA8F83413ull,0xF77292CEC4874F2Aull,0x8946338928EE6055ull,0x46E65C6713AA5B6Full,
            0xD65F14C04A6272DAull,0x63F49F6E5D093F38ull,0x64EB0B3FDF2E744Bull,0xC80D088BB206BAA1ull,
            0x6CF5F98CED84D2E8ull,0x2D1632FD0705FB23ull,0xA7053E7B20187900ull,0xC98E15E619152A6Bull,
            0xBD670D35D510112Bull,0x3A2A386C5971D381ull,0x4BC51593411D731Eull,0xC23D1BEF48518EC4ull,
            0x9BF508563E8E77EEull,0x8C1FA9D5DFBFFE3Eull,0xD5AD3009A3730515ull,0xDFEECEC5C687EFDFull,
            0x826A45EC71F8880Full,0xE3A45E85468FC64Cull,0x3CC74FF6693CC3F5ull,0xFAE0787EF4E35EE0ull,
            0x937560EEEA97F524ull,0x5114D4FE7EF8845Dull,0xA8D6DBD9AF85DEB3ull,0x94AA1FD17034C881ull,
            0x329D9626E85808DFull,0xF50EAED3548BF679ull,0x277C80F3BFF34CD1ull,0xEAC88DBBF22DFFADull,
            0x52FC06A3E9D39C4Bull,0x57FED40E5B49853Bull,0xDF0A2D9047CB1EF4ull,0x6F3371FC42B4BFFAull,
            0x215A7B757D4945CDull,0x5B65B30BACCB7268ull,0xE954E02E1DA00E7Cull,0x9B29B207708F6EB0ull,
            0x48BC96FE055B322Aull,0xF35BE87E41BBD67Aull,0x8F14538BAD64512Cull,0x41A99C8660DBB042ull,
            0x838340020DA3FAD9ull,0xF7AFC4C520E23F53ull,0x701EFA30E123E16Bull,0x2985A7F7B4F31324ull,
            0x68D961D35D0CD183ull,0x3344CBE796AD037Eull,0x0789D7D49CE7DB1Aull,0x8BBE530097080AFAull,
        },
        {   /* white rook */
            0x7632F448B1273C53ull,0x2C429A7BE671ABABull,0x9B5D129320BA7632ull,0xE0F23BD244DA7C38ull,
            0x449EDB04B453F0F7ull,0x4A8BF9D18BF71B34ull,0x9355C51BEF8EFD53ull,0xF106311B773B3934ull,
            0x883C1E87C3220AF5ull,0xC3607C982CA8DF9Eull,0x8DEA7E642DB2C17Full,0x3337B40679064E96ull,
            0xAF4C92C1BEF8ACCEull,0x0E8B477B36373600ull,0x676D5D966C640BDEull,0xA056BF2FB97C0F7Dull,
            0xF64D5E067B86A434ull,0x4594311523BE8944ull,0xCB347028B9DA40C2ull,0xF3546AD91915EE40ull,
            0x265F8D449595343Aull,0x4D980A926638F783ull,0x57682E65CA73D00Aull,0xA547A7DCC7D9C931ull,
            0x3882D641298AA5B3ull,0x3FB9A9A423D0BE56ull,0xFAC92F28454A26BFull,0x3F01BDDA06FF8832ull,
            0x201BE11EE258BED9ull,0xD665EFC09B5DDC76ull,0xF388264C0F395254ull,0x17BFF2B8C501CE11ull,
            0x6EC204A5C7B1F7E3ull,0x17498DDBA59BF96Cull,0x5196EA2CE2AA79F5ull,0x643053B97F56480Aull,
            0xDD61748310E15A1Aull,0xE06485A3AFDC30BAull,0xD55E6AF24C2D2EF8ull,0xCCC5244848C10868ull,
            0x3A69FFC1D11D0118ull,0x9132A60AFB14CA84ull,0x30F01298B79A1579ull,0x7FD721DBEC46A268ull,
            0xC0C76C3967035C6Aull,0x5C7A974FAFEEB62Aull,0x21550B396EA1F828ull,0xFAFA1C217830FD1Eull,
            0xFBBEA6310B3281A5ull,0xB704243FAD934F23ull,0x8C3DABF880B79E65ull,0x42793257EF4CB74Full,
            0xF368DB8799168733ull,0xB225D7D0041F18A2ull,0xB8827504A9BE4E08ull,0x79D66426205B2604ull,
            0x47E5B5C73F2AF8DBull,0x060364A793770515ull,0x0A1FF12E23AD0CE9ull,0xEDC26ED8C962364Full,
            0xCEE736A0D3EB0EE2ull,0xCCE0FD3A0FC3645Full,0x521BC6965AC6C3FFull,0xE8F9E2875927DB2Cull,
        },
        {   /* white queen */
            0x97E0D3B6FAE32444ull,0x6F1D32876301F463ull,0x9F79B9F61C82935Aull,0x517B1CB0F6661A66ull,
            0x346B337C8E98BDE9ull,0x11632483F96F9902ull,0xFCFCF6ED60D4FF8Dull,0xBED243A1FC1345AAull,
            0x589D2BFA90627DB8ull,0x686D3360033C5010ull,0xF70CAAB1F25A4FB3ull,0xFE0626CE422C3AF3ull,
            0x40FE45F531E5F242ull,0xAE67AC37731BCBC7ull,0xDF223AB3B9CB299Dull,0x12F367EB7F123ADAull,
            0xE2882630C47E2063ull,0x04FA8BCC64C87EBDull,0xA7C034E904EA546Bull,0x3D5E0BB21A9241C1ull,
            0x55D96E1D16C0ED3Eull,0x469F3CE5838D0BD1ull,0x7492B65D8C5A024Cull,0x613E318254575DDBull,
            0x43CF7CBD1F2E5E97ull,0xA5CFC2E3857D275Cull,0xF60961E3E1572D4Dull,0x19E663DFDA045CD6ull,
            0x9D1A32879C42D54Eull,0x3A2C233AF3E54F2Aull,0xAB3F1692269E00ECull,0x8B635E7865DB1308ull,
            0xAC6829B6636A3F5Cull,0x115258E645343325ull,0xE9F700EB11F1D3C1ull,0x54D66BFD9AD5D2ECull,
            0x22329F6D3648A093ull,0xEF0A9070BEE5ABCFull,0xC1C229C3A70C8DD7ull,0x28BF9D94C85A269Bull,
            0x2CF38514C929FF5Full,0x1561520FBBFBCE84ull,0x7846A9B30A5C7203ull,0x68AF08EBCA265361ull,
            0xD4F42221623ECB0Aull,0x3EE386BE1E763C7Aull,0x4E817A9A1569C683ull,0x86135B81E2BD1AE4ull,
            0x50B8921F066B4076ull,0x2EA50851B045F2DBull,0x5C5F409C40D61F8Full,0xAEEDB297BA5EF8BEull,
            0xD4D6D90C00D1C618ull,0x9043DFFD10D82B08ull,0x9947A784808E3338ull,0x1237EEEE389BA77Eull,
            0x95EC1DD7A6A28B51ull,0x42491C090DF95700ull,0xE12936AAB81B4929ull,0xB1C69296AF2B5E51ull,
            0x23DE4402AF11A2AFull,0x6A63ABD8C136987Full,0x80153698D9DD1CA1ull,0x7B2A38F4C0C571F4ull,
        },
        {   /* white king */
            0x25A64E025145AC2Aull,0x471C089E02274BB7ull,0xCE93825AB37CBC9Cull,0xAE7C98A4C4C2F33Bull,
            0xE769AE6E747E8226ull,0x2462B90F63CCC73Aull,0x014C88AC1726D1ADull,0x67518870E51B3CC9ull,
            0xA77BD79EA3496E2Eull,0xB7E2D51175CC4C0Eull,0xDFC89A04395C80BBull,0x0517829E96C5C1E7ull,
            0x93837CD56EFAACD5ull,0x71F8CF43484C1516ull,0xE386AF8DA2BA445Bull,0x1B506BBB9A60804Cull,
            0x1D262B34C3F91E8Eull,0x93E20049882C9793ull,0x37DB2CC0CAAB3156ull,0x1102D3C7A27D19F4ull,
            0x6B86DEF9491C75C5ull,0x01E590412838E900ull,0xD68B82B92EA8C7BEull,0x7B6A538C8170027Dull,
            0x6B50646788D6100Bull,0x6613AEBFC976FE0Bull,0x233D84629CDCE7ABull,0xFB51578711E8FBA5ull,
            0x9CE245E6F5DBBA28ull,0xE244A45EBB232CD5ull,0x54BBF5B42C0BEDDCull,0x05DD0D532B79227Dull,
            0x379BCBD676B22711ull,0x0FE24BBB9B47106Dull,0xE85464C197863B52ull,0x398A8E54F8249630ull,
            0x05F4EF8FD55DE414ull,0xAE67FB66CB414683ull,0xF0F1E4915CDC8E5Cull,0xC8AC0B439B892C9Bull,
            0xBBB2E67BC782198Bull,0x99D876769B0E16C7ull,0xC35479296CCE9A80ull,0x975C5FD31FD87909ull,
            0xBEBA7621562BCE15ull,0xA1D5A800A0474D9Cull,0xA1918FA632B48153ull,0x2F3B5F0E6C5E8CE1ull,
            0x73BE96C21AF6D1D2ull,0xCC79437913A1AD76ull,0xA3B7C9CD0294AD89ull,0xFAFE96F22317A8A9ull,
            0xF972FA252D37946Aull,0x019BCD8847A394FEull,0x23A50F6812D02ABEull,0xB0FFF5ECAAA0AE47ull,
            0x985363B3D23AAC7Dull,0x946DCEF0D65DB49Dull,0x112FAF41835414F5ull,0xC5C71C80D43A1E42ull,
            0x8B1F6B26EFE9E762ull,0x0DB7576008C828B3ull,0xFFA54AB8DF573F99ull,0xCE90EBCD64C4ADC2ull,
        },
    },
    {
        {   /* black pawn */
            0xDFD4E8E06D33365Aull,0x5A377BDE20F8AD0Aull,0x8925EFBC5BDD5CD6ull,0x67DDBC6EC4D1EA78ull,
            0xCE87FEB269AF01A6ull,0x3F9863552263E97Dull,0xCB30FD8912DF4B1Eull,0x2CCBCAE58F9EBA42ull,
            0xBFDBA0082E3626BEull,0x3BE926F1803E52E1ull,0xABAE57DA102443F9ull,0x56544A13EC0A8DC8ull,
            0xC86ABDC98907B050ull,0x1B82491A20AEC694ull,0x21F54D5AE174A624ull,0xD0AE582FB1C446CAull,
            0x526E93F5CED4F8A2ull,0xDF13D2DF6D5CED96ull,0x8E10C8B96081B8D0ull,0x41E428038C2E8206ull,
            0xF7A867BE7C2BBE3Full,0x2839AF4677173E03ull,0x095301EE94B61977ull,0xE1B9707186DAD1DBull,
            0xBB434C2D067558AEull,0xAA8B8EA3B1DC4AD4ull,0x958D174EA6D9E4DAull,0x18FA9FCF3AAB1A4Eull,
            0x50E56EDD79491074ull,0x170581F4420CE9ABull,0xB218C43FEC918C84ull,0x198614831D9B3D4Full,
            0x4BB804FAC03004E2ull,0xC925A23AF767A1AEull,0x27A1076EF4626C62ull,0xCDBBA934E63D4657ull,
            0x2173F829223947DAull,0x54500385260034DEull,0xC5360B952C96DF18ull,0x405464BDC69E201Dull,
            0xC18AE202F3EAD0D4ull,0xE95BB1F8A98A14EAull,0x899E1E8B6E615D4Dull,0xEBF71600EA9E0B75ull,
            0x8696C17B4C591ECEull,0x37B42E2B33744CA4ull,0xE77F53B73552B528ull,0x87D692650F4EEEC0ull,
            0x3560B490FEDC2C0Full,0xD3F0CAC6D77D2FCBull,0x633CFE7AFD830277ull,0xD12CE88BB953CB22ull,
            0x2BC4B375517DBF50ull,0xD044BBEDC944E5EAull,0xFC528D389CAC99ECull,0xC3F9902CF7463384ull,
            0x79A7C523DAD80E2Cull,0x46D82F37540FDAFCull,0x7390F4736613BB23ull,0x6F62D7AC79F683DAull,
            0x830DB8AB3182C77Aull,0x71C517D719A08ED6ull,0xFC7F74A0F06D0186ull,0x4E54E62FE521FA88ull,
        },
        {   /* black knight */
            0x35D465DB5A844500ull,0x73CE3BA30CC70308ull,0x50EEC7379F6664BFull,0xF111D6601B35EE50ull,
            0xAED40BF9C1F2673Bull,0x62BEBCDD52BBAE4Bull,0xA64ABA0C66196675ull,0x83DCD181D480D150ull,
            0xC0D64E35F405EFA9ull,0x34B6F5EE24874EC0ull,0xCCBECA1A4A7C2CA1ull,0xB6C530EB3BC667ABull,
            0xB9B01FFA29A6F20Bull,0x40B868262769ABA2ull,0x8569E62707AD67F6ull,0xC53EF38DC12085BDull,
            0x5C178F1D81F4278Dull,0x402901566AE72BA0ull,0x4C06C66B6EC75041ull,0xBF9145770DA70138ull,
            0x106DC6C17BD577ECull,0x2201ADF85F52C1BEull,0x658B4CD94634C67Full,0xA33BF7EC21DA626Cull,
            0x5DC865F5F0EBFEF0ull,0x438D14B12B920FD0ull,0x8F9621DB0377891Aull,0x93591F4FDF569689ull,
            0x1A1147FB2233D241ull,0xB515546C8FF241D6ull,0x57E6808269A8144Aull,0xA24B34AF7BD98440ull,
            0x947B677277172A2Bull,0xBFC75F9E8DEAAFA3ull,0x54FBBCFEA6F75087ull,0x292B528B998FEE9Cull,
            0x4CB584AEF94A0F62ull,0xA4B3D4A71BC9ADD8ull,0x133089EEBDA49ED3ull,0xF816A655AEA9A42Eull,
            0xD3B157FB47510BA6ull,0x4592D7E8159267CCull,0x0BDA5E65EEE1D762ull,0xB3A7F526E7D75C93ull,
            0x9EFDD3FBDF171236ull,0x929ADAC27DFAE550ull,0x803975D8ECA3F3C7ull,0xADC8B5942588A16Aull,
            0x7C928E4696589A62ull,0x63585C2525930518ull,0x7A30D44ABBDE0B80ull,0xCD16CFE1184BF593ull,
            0x2847FBC990E0FA24ull,0x1BEA1EECDCD3A0D9ull,0xB453C784FFD8CCA7ull,0xC22925E4F17E8980ull,
            0xC1917E88192E2903ull,0xB4799188240C7087ull,0x4428449000A96410ull,0xF28ABB6B74347308ull,
            0x72F19F103AB46D9Bull,0x8E04F963E3A625DCull,0xA5BAA481617FD947ull,0xE0CD6C10AD41B012ull,
        },
        {   /* black bishop */
            0x447804A3148BA175ull,0x24BEA72A538C99D3ull,0x77626D6B7D43EC3Cull,0x3DECB687E8E16090ull,
            0x7B820212533DD62Full,0xEC8B6234D271B85Aull,0xF9293BE5EF9E27ABull,0x021C83448C07A5BEull,
            0x1D636B418A51D1FBull,0xF64C0B3205E55607ull,0x36CCA4CCD0D05D1Cull,0xBB10CF1D9813467Full,
            0x5B00815FD820BCB2ull,0x28137633D5BB8FC1ull,0xE06B859B367B36AAull,0x329F42EED6EC4B94ull,
            0x76D0D039992A47FFull,0x23DD531A743258F5ull,0x4829927D369554FAull,0x4CE70496EA39160Eull,
            0xEDC9F135AA4C6C69ull,0x8D2A590C87E4A911ull,0x25AFF118D9FC06A8ull,0xE0E58BA2A937E158ull,
            0xE5A81C689EACD609ull,0x2F9A344D1801282Dull,0x9163A79212EB6761ull,0x32428E0A4E6A1667ull,
            0x6411D6EF8C151C40ull,0xC2ED78AC3EB9144Aull,0x1D546F6AA48820D8ull,0xE0857BF9F43F3882ull,
            0x79508FB6B5585393ull,0x8E9C1EEE8AB8654Eull,0xEA4E557376F11A63ull,0xCED601475376AF7Bull,
            0x5F74A6D3242B6557ull,0x63D5BFBF89FBF61Cull,0xFC758A8BAEA2B593ull,0x26894F30D9907172ull,
            0x9B3B283141BF3237ull,0x822A7FFC11285845ull,0x42ACB8439873A2F5ull,0x94B3DA7065DB078Dull,
            0xDEDB84C86DF008EBull,0xD7E7D89EA976A6A2ull,0x32D5E1AA8763B779ull,0x717DF14EA38A1710ull,
            0x45A6F055512775E4ull,0x25D0ED30160A3765ull,0xC92B00708F2FC4ABull,0x076FDD76F5D0E559ull,
            0x571E9058F1B1BE14ull,0x87FFD42293CDB6A7ull,0x2B2D653EE4F271FFull,0x3CEDEEED355E79D5ull,
            0xBF258ECD5178EBC1ull,0xFABA4E58ED6F6E7Eull,0x42C79B039279BC9Full,0xCF6874E799DB0641ull,
            0xF59157A52A83FA77ull,0x4BA2682F8149AF7Aull,0xCE3BA67CCB3D7C60ull,0xDCDB384F6E060C05ull,
        },
        {   /* black rook */
            0xC3D6A45CE867FD77ull,0xEBB972927D616748ull,0x8DAF70EC6C6B1B42ull,0x60681E9FC6CBE67Eull,
            0xFA3F0647A18A5D3Bull,0x68EB4066977CE3F2ull,0x9FFA940CA1050EB1ull,0x319A68D3E6DF96C3ull,
            0xA5D0E9C340ED935Aull,0x1FCA885BDB55A7DFull,0xFB58BAE7935B037Dull,0xB62C7BE92C8C88CAull,
            0xC0DEA334EC8C3B7Bull,0x4D8903C505456786ull,0x068C96D4076C7390ull,0x9CA125623C3684ACull,
            0x258BFB0F28BF07FCull,0x256CBD641A261F5Cull,0x0AA0BF754A4C3D8Full,0x7D64066FADD3308Eull,
            0x89FF10DD97B181D3ull,0x207BE71DBC495D04ull,0xDF513528FFF9247Eull,0x9E11CAA30BD49A59ull,
            0x20E4DD9C30300CFEull,0xC4CEB6EA34FDFBFAull,0xFA5C6098FD9C9820ull,0xA507FB466ADDA46Eull,
            0xC9611E28CC41660Eull,0x8039AD94E91A51F8ull,0xDF10D47C3E0FA6A3ull,0x9C3C850598B36766ull,
            0x815E45DBADD96E5Eull,0x0618B4C9BC9A5805ull,0xD4CA7DDD06B587DEull,0xED117F4B3BBCC083ull,
            0x8799947E2D7DF685ull,0xEFF6CF318FF7B07Eull,0x4A25443D491FD821ull,0x4AE3D59BF8D9DB63ull,
            0x8AAB17A78A18C8B9ull,0x462191D237BAACBFull,0x07987ACC2FC89740ull,0x7E698749842BB615ull,
            0xF46BAA2016544CFEull,0x69B09A11760C467Full,0x30875E7328F90BAAull,0x6550835F462ABEBDull,
            0xB09BD6A156375D86ull,0x4B5D71424BB58F65ull,0xF39ED7BB0C5B5C26ull,0x279C8DB36826EF8Dull,
            0xA0D30B4766DFED1Full,0x2A0E5F24756A8546ull,0x5312983F855E348Cull,0x4E52EA16816D536Cull,
            0xE77983C075F47C54ull,0xDCF91584061ECDD8ull,0xAEDC37AC75BBDBA9ull,0xAD915AAA5E46F5C9ull,
            0x72C87B0122C0329Eull,0xC70750F2EC537D0Eull,0x5ED80BC95639D48Bull,0x2B8B9245E14E514Aull,
        },
        {   /* black queen */
            0x809AA1C92CF34835ull,0x054086B8E8449CF9ull,0x79C78704A8504DF8ull,0xADB2340700D1C736ull,
            0x32E3AECF3274ADF1ull,0x4E2A778D85DFFBC8ull,0xA6A147C7DC2E47C1ull,0x76454CCF74E8E9D5ull,
            0xCF5F14CFE85D65CDull,0xE16CA04521933849ull,0xD51C884FA2D7C606ull,0x9BBAFADA4763917Dull,
            0x228FBDF97461B66Eull,0x88B26DB0FB3A993Dull,0x5CE37208207EB378ull,0x7671B98BE3B3C8A8ull,
            0xE3875C33B09B482Full,0x4F1E2C05DD2F89DEull,0x05F2961559D69431ull,0xED4D549923753D1Cull,
            0x8E8C6382CB6529FDull,0x53428CAD5CBE78FBull,0x677D9A08CB6263B2ull,0xE7E8D6353DB39204ull,
            0x9DC1AAB439DD7506ull,0x9EDE0D3469B20208ull,0xD231C89342127A88ull,0x911D4E44E42F009Aull,
            0x3ED92A71AA2FD24Cull,0xCBAB975B5100C38Bull,0x6DFACEE3A2068303ull,0xA9A6C1A1B3FF6E8Bull,
            0xE3F9E70E98898ACBull,0x1C3A40040A5B40C7ull,0x32DAD01DEBC210B8ull,0xAFB87053973D93F0ull,
            0x8EA2AB302E9B13E7ull,0x3ACD82DA55748FF0ull,0x3D5953F9960D336Aull,0x50CCAA48725C7521ull,
            0x0FA8519B5A5C2DE5ull,0xE055ADE29EDBEC4Aull,0xD774E18BD78FC16Eull,0xF17EFCA7D23A0123ull,
            0x724AB59E14C9E276ull,0x0BC19E4DB2DE9F39ull,0x0F987A02822C58CCull,0x73032E7F55BC2C6Dull,
            0x0A3A6B007B5F9696ull,0x7F09E72FB41CD683ull,0x8BE407435C26779Full,0xE293A20B0B71966Cull,
            0x7E4BE2E95781C894ull,0x4EA7B613AA665C12ull,0x20F5E7B4AF4A7B46ull,0x471B886A1E11508Bull,
            0x5198D206F645F638ull,0x1CBDD92D47C974ECull,0x780B75C3C1F4267Dull,0x8E6ADAB46DB9F41Cull,
            0x3FEA63BAD635096Bull,0xF2659F5E10C424F6ull,0xB5CB24D7FB994298ull,0xD603CFD17EECB1BCull,
        },
        {   /* black king */
            0x828D7C8FFDE12FE8ull,0xBA7C1BEB334EE4DFull,0xCEE75AEC89B1C421ull,0x87CF9E59D45F2F37ull,
            0x14EB7733BDB929A9ull,0x59DB12AED5F86284ull,0x900F7AEAD57BEB55ull,0xD26DAC40B7E10925ull,
            0x8E0312A16B5B1301ull,0x08B0225EC4B25C24ull,0x4F9CCE4BD1E7CC4Aull,0x724AA6E7A8AA4B2Dull,
            0x56325A50C262F35Aull,0xC4F459035AD7D660ull,0xC5AC8C913F293770ull,0x73AE32DAB1A9A5D2ull,
            0x2196F1D50B7528EAull,0x88271EA5A63DD789ull,0x95835CC4E904EA5Bull,0xAA814F9780874F1Cull,
            0x698BD8C237457B65ull,0x73437FB9636607E0ull,0x117491D65D3EEFFCull,0x59BDD2F12DCDD36Dull,
            0x829DE6C5613C76EDull,0x2641250C64EAC3E6ull,0xD4A4E480C7F6C5B1ull,0x97B4CC1E99240B8Dull,
            0x11CBFE33970C8DC6ull,0x52E06481ACFC62FBull,0x36B9705C00C8FD3Aull,0x09D29CF7001C2C50ull,
            0xA8D64A5A1F0F3C3Eull,0x12C27209E5AB0A7Bull,0xF420FE3580C048FBull,0x220C35CE9DC25CE9ull,
            0xE8B2223AB8B4E742ull,0x4AFA373D0196EBF6ull,0xC7524B3BC6613832ull,0x7310533773DAA469ull,
            0xBFE9491D5A84BE44ull,0xB841FB135288E5BFull,0x8E538F611BE30EB3ull,0xE323F98498D3C6C0ull,
            0xEBCFFBB54C1BDF2Bull,0x29AA2A954A906641ull,0xC428026AEBBB7533ull,0x05945FDAE9A6FA9Full,
            0xCDD54DB60DEEFBB5ull,0xFB0D5E12453E0A59ull,0xC04FAC271B76E80Cull,0x3E55A183FA5F9DD2ull,
            0xB44B8CBD17758D6Full,0xD6C0D366F6F51F48ull,0xA8FE1AE6B3D33F77ull,0x55D67881C2974EC7ull,
            0x0A6DDEFB287B461Bull,0x9860FAE628A28C5Dull,0x6E1BF3309C53F1E1ull,0x3F2BB945D2C5DEA7ull,
            0xB47DC0F782467DBEull,0x1EC5A106E867F596ull,0x66FA5EF51869656Full,0x2F644AA8FB4C14B1ull,
        },
    },
};
const uint64_t CASTLING_RIGHTS_HASHES[16] = 
{
    0x887A04B735D9554Full,0xEE890974F92F3FE5ull,0x4A254CB66FE9FC43ull,0x0E86E2CACD6457EFull,
    0x7688DBECA794A71Aull,0xEB80AA0B2F6D5246ull,0xF7B6EEBBB48B913Dull,0xE9036275CDE37AD2ull,
    0xA4C45A9DD611AF60ull,0x767C56E065588AF1ull,0x31B180F3EEF55056ull,0x7317E9F697392288ull,
    0x0BCA8B3C3284A2A5ull,0xCAF25E6BD124FBBDull,0x4C486425CF030942ull,0xE28DB2D61A4F4858ull,
};
const uint64_t EN_PASSANT_HASHES[8] = 
{
    0xAED2A5C36C13AFAAull,0x2788A0D04ED2EDD2ull,0x52FA345DF5F9DB4Full,0xAA71D59324833C58ull,
    0x07812A44C4679AD0ull,0x7407EAAEF22B86BAull,0xA4E17CAAC1E9A3A4ull,0xFD8E57C4E7046224ull,
};
