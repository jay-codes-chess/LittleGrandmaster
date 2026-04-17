#include <stdlib.h>
#include <stdio.h>
#include "bitboard.h"

//=============================================================================
// Precomputed Tables
//=============================================================================

U64 SQUARES[64];
U64 FILES[8];
U64 RANKS[8];
U64 KNIGHT_SPAN[64];
U64 KING_SPAN[64];
U64 PAWN_ATTACKS_W[64];
U64 PAWN_ATTACKS_B[64];
U64 CENTER;
U64 BIG_CENTER;

// Magic bitboard data
static const U64 ROOK_MAGIC[64] = {
    0x8a80150000800000ULL, 0x128000802000ULL,    0xc00040004000ULL,  0x1000100018000ULL,
    0x4000400080004ULL,    0x200030003800ULL,    0x100000e000000ULL, 0x802000008000ULL,
    0x8008004000300038ULL, 0x40000880000ULL,     0x400080400000ULL,   0x400030030000ULL,
    0x18000800a000ULL,     0x200010009800ULL,    0x20000200020020ULL, 0x1000004000800ULL,
    0xb000200400080ULL,    0x100001000040ULL,    0x1000102000800ULL,  0x400030201000ULL,
    0x4020014000ULL,       0x200000200010ULL,    0x840010008000ULL,   0x2000004020ULL,
    0x800002010080ULL,     0x2002000400040ULL,   0x1020008000800ULL,  0x30040004000ULL,
    0x8000080010ULL,       0x200001002000ULL,    0x800800010010ULL,   0x2002020001ULL,
    0x400080200ULL,        0x1080010008000ULL,   0x2004001000ULL,    0x200000800080ULL,
    0x800200800400ULL,     0x8000100201000ULL,   0x4001000010000ULL,  0x40200000800ULL,
    0x8000400800100ULL,    0x2000200010000ULL,   0x2000100100000ULL,  0x1000200030000ULL,
    0x40004000008000ULL,   0x8000000010000ULL,   0x1000000020000ULL,  0x8020000004000ULL,
    0x4000000001000ULL,    0x1000400000000ULL,   0x4000002000000ULL,  0x820000000100ULL,
    0x804000000200ULL,     0x408000000400ULL,    0x204000000800ULL,   0x102000001000ULL,
    0x800800000040ULL,     0x200008000100ULL,    0x802000000080ULL,   0x80004000800ULL,
    0x200000200200ULL,     0x100100001000ULL,    0x400008000800ULL,   0x400008001000ULL
};

static const U64 BISHOP_MAGIC[64] = {
    0x400800040010000ULL,  0x40200010040000ULL,   0x10020040080000ULL, 0x40200008020000ULL,
    0x30010000802000ULL,   0x1000400080000ULL,    0x20020020001000ULL, 0x8020000801000ULL,
    0x4000802000a000ULL,   0x20000808010000ULL,   0x10001080020000ULL, 0x10400040020000ULL,
    0x10002008010000ULL,   0x8000802008000ULL,    0x1000010040800ULL,  0x4040000402000ULL,
    0x2010008004000ULL,    0x1000080802000ULL,    0x4020001002000ULL,  0x2040000801000ULL,
    0x1020000400800ULL,    0x200100802000ULL,     0x800080204000ULL,   0x1040004002000ULL,
    0x2010008004000ULL,    0x200800802000ULL,     0x400100204000ULL,   0x400080101000ULL,
    0x201000800800ULL,     0x202000800400ULL,     0x100200080400ULL,   0x104080010200ULL,
    0x102000800400ULL,     0x80080080200ULL,      0x400200020080ULL,   0x10080020100ULL,
    0x202001000800ULL,     0x200800201000ULL,     0x202010020080ULL,   0x20008010200ULL,
    0x1000010200800ULL,    0x200020002000ULL,     0x408000040100ULL,   0x100200200080ULL,
    0x804000010200ULL,     0x100200101000ULL,     0x202000100800ULL,   0x102000080400ULL,
    0x100800100400ULL,    0x10200080200ULL,     0x200001002000ULL,   0x80080020200ULL,
    0x400010100400ULL,    0x402000080400ULL,    0x200400080800ULL,   0x200400100400ULL,
    0x800040080200ULL,    0x100010100200ULL,    0x800200100400ULL,   0x400010200200ULL,
    0x4002001801000ULL,   0x8020000200800ULL,   0x4001000100800ULL,  0x804000100400ULL
};

static const U64 ROOK_BLOCKER[64] = {
    0x000101010101017eULL, 0x000202020202027cULL, 0x000404040404047aULL, 0x0008080808080876ULL,
    0x001010101010106eULL, 0x002020202020205eULL, 0x004040404040403eULL, 0x008080808080807eULL,
    0x0001010101017e00ULL, 0x0002020202027c00ULL, 0x0004040404047a00ULL, 0x0008080808087600ULL,
    0x0010101010106e00ULL, 0x0020202020205e00ULL, 0x0040404040403e00ULL, 0x0080808080807e00ULL,
    0x00010101017e0100ULL, 0x00020202027c0200ULL, 0x00040404047a0400ULL, 0x0008080808760800ULL,
    0x00101010106e1000ULL, 0x00202020205e2000ULL, 0x00404040403e4000ULL, 0x00808080807e8000ULL,
    0x000101017e010100ULL, 0x000202027c020200ULL, 0x000404047a040400ULL, 0x0008080876080800ULL,
    0x001010106e101000ULL, 0x002020205e202000ULL, 0x004040403e404000ULL, 0x008080807e808000ULL,
    0x0101017e01010100ULL, 0x0202027c02020200ULL, 0x0404047a04040400ULL, 0x0808087608080800ULL,
    0x1010106e10101000ULL, 0x2020205e20202000ULL, 0x4040403e40404000ULL, 0x8080807e80808000ULL,
    0x7e01010101010100ULL, 0x7c02020202020200ULL, 0x7a04040404040400ULL, 0x7608080808080800ULL,
    0x6e10101010101000ULL, 0x5e20202020202000ULL, 0x3e40404040404000ULL, 0x7e80808080808000ULL,
    0x7e0101010101017eULL, 0x7c0202020202027cULL, 0x7a0404040404047aULL, 0x7608080808080876ULL,
    0x6e1010101010106eULL, 0x5e2020202020205eULL, 0x3e4040404040403eULL, 0x7e8080808080807eULL
};

static const U64 BISHOP_BLOCKER[64] = {
    0x0040201008040200ULL, 0x0080402010080400ULL, 0x0100804020100800ULL, 0x0201008040201000ULL,
    0x0402008040200800ULL, 0x0804008040200400ULL, 0x1008008040200200ULL, 0x2010008040200000ULL,
    0x0020100804020400ULL, 0x0040201008040800ULL, 0x0080402010081000ULL, 0x0100804020102000ULL,
    0x0201008040204000ULL, 0x0402008040202000ULL, 0x0804008040201000ULL, 0x1000008040200800ULL,
    0x0002080402010400ULL, 0x0004100804020800ULL, 0x0008201008041000ULL, 0x0010402008042000ULL,
    0x0020804008040400ULL, 0x0041008008040800ULL, 0x0082008008041000ULL, 0x0100008008040200ULL,
    0x0000040804020400ULL, 0x0000081008040800ULL, 0x0000102008041000ULL, 0x0000204008042000ULL,
    0x0000408008040400ULL, 0x0000800008040800ULL, 0x0000000008041000ULL, 0x0000000008040200ULL,
    0x00000004020400ULL,   0x00000008040800ULL,  0x00000010081000ULL,  0x00000020082000ULL,
    0x00000040080400ULL,   0x00000080080800ULL,  0x00000000081000ULL,  0x00000000082000ULL,
    0x00040204000000ULL,   0x00080408000000ULL,  0x00100810000000ULL,  0x00201020000000ULL,
    0x00402008000000ULL,   0x00804008000000ULL,  0x01008010000000ULL,  0x02010020000000ULL,
    0x04020400000000ULL,   0x08040800000000ULL,  0x10081000000000ULL,  0x20102000000000ULL,
    0x40200800000000ULL,   0x80400800000000ULL,  0x00801000000000ULL,  0x00200000000000ULL,
    0x40204000000000ULL,   0x80408000000000ULL,  0x00810000000000ULL,  0x00200000000000ULL,
    0x00000000000000ULL,   0x00000000000000ULL,  0x00000000000000ULL,  0x00000000000000ULL
};

// Attack tables
static U64 ROOK_TABLE[102400];
static U64 BISHOP_TABLE[4096];
static int ROOK_SHIFT[64];
static int BISHOP_SHIFT[64];

//=============================================================================
// Forward declarations for naive attacks (used during table init)
//=============================================================================

static U64 rook_naive(int sq, U64 occ);
static U64 bishop_naive(int sq, U64 occ);

//=============================================================================
// Initialization
//=============================================================================

void bb_init(void) {
    // SQUARES
    for (int i = 0; i < 64; i++) SQUARES[i] = 1ULL << i;

    // FILES
    for (int f = 0; f < 8; f++) FILES[f] = 0;
    for (int f = 0; f < 8; f++)
        for (int r = 0; r < 8; r++)
            FILES[f] |= SQUARES[SQUARE(f, r)];

    // RANKS
    for (int r = 0; r < 8; r++) RANKS[r] = 0;
    for (int r = 0; r < 8; r++)
        for (int f = 0; f < 8; f++)
            RANKS[r] |= SQUARES[SQUARE(f, r)];

    // Knight attacks
    for (int sq = 0; sq < 64; sq++) {
        int c = FILE(sq), r = RANK(sq);
        U64 attacks = 0;
        static const int off[8][2] = {{1,2},{2,1},{-1,2},{-2,1},{1,-2},{2,-1},{-1,-2},{-2,-1}};
        for (int i = 0; i < 8; i++) {
            int nc = c + off[i][0], nr = r + off[i][1];
            if (nc >= 0 && nc < 8 && nr >= 0 && nr < 8)
                attacks |= SQUARES[SQUARE(nc, nr)];
        }
        KNIGHT_SPAN[sq] = attacks;
    }

    // King attacks
    for (int sq = 0; sq < 64; sq++) {
        int c = FILE(sq), r = RANK(sq);
        U64 attacks = 0;
        for (int dc = -1; dc <= 1; dc++)
            for (int dr = -1; dr <= 1; dr++)
                if (dc || dr)
                    if (c + dc >= 0 && c + dc < 8 && r + dr >= 0 && r + dr < 8)
                        attacks |= SQUARES[SQUARE(c + dc, r + dr)];
        KING_SPAN[sq] = attacks;
    }

    // Pawn attacks
    for (int sq = 0; sq < 64; sq++) {
        int c = FILE(sq), r = RANK(sq);
        PAWN_ATTACKS_W[sq] = 0;
        PAWN_ATTACKS_B[sq] = 0;
        if (c > 0 && r < 7) PAWN_ATTACKS_W[sq] |= SQUARES[SQUARE(c - 1, r + 1)];
        if (c < 7 && r < 7) PAWN_ATTACKS_W[sq] |= SQUARES[SQUARE(c + 1, r + 1)];
        if (c > 0 && r > 0) PAWN_ATTACKS_B[sq] |= SQUARES[SQUARE(c - 1, r - 1)];
        if (c < 7 && r > 0) PAWN_ATTACKS_B[sq] |= SQUARES[SQUARE(c + 1, r - 1)];
    }

    // Center
    CENTER = (FILES[3] | FILES[4]) & (RANKS[3] | RANKS[4]);
    BIG_CENTER = CENTER
        | (RANKS[2] & (FILES[2] | FILES[3] | FILES[4] | FILES[5]))
        | (RANKS[5] & (FILES[2] | FILES[3] | FILES[4] | FILES[5]));

    // Magic bitboard tables
    for (int sq = 0; sq < 64; sq++) {
        ROOK_SHIFT[sq] = 64 - __builtin_popcountll(ROOK_BLOCKER[sq]);
        BISHOP_SHIFT[sq] = 64 - __builtin_popcountll(BISHOP_BLOCKER[sq]);

        // Rook
        int rcount = 1 << __builtin_popcountll(ROOK_BLOCKER[sq]);
        for (int i = 0; i < rcount; i++) {
            U64 occ = 0;
            U64 mask = ROOK_BLOCKER[sq];
            for (int j = 0; j < __builtin_popcountll(ROOK_BLOCKER[sq]); j++) {
                int bit = __builtin_ctzll(mask);
                mask &= mask - 1;
                if (i & (1 << j)) occ |= SQUARES[bit];
            }
            int idx = (int)((occ * ROOK_MAGIC[sq]) >> ROOK_SHIFT[sq]);
            ROOK_TABLE[(sq << 10) | idx] = rook_naive(sq, occ);
        }

        // Bishop
        int bcount = 1 << __builtin_popcountll(BISHOP_BLOCKER[sq]);
        for (int i = 0; i < bcount; i++) {
            U64 occ = 0;
            U64 mask = BISHOP_BLOCKER[sq];
            for (int j = 0; j < __builtin_popcountll(BISHOP_BLOCKER[sq]); j++) {
                int bit = __builtin_ctzll(mask);
                mask &= mask - 1;
                if (i & (1 << j)) occ |= SQUARES[bit];
            }
            int idx = (int)((occ * BISHOP_MAGIC[sq]) >> BISHOP_SHIFT[sq]);
            BISHOP_TABLE[(sq << 6) | idx] = bishop_naive(sq, occ);
        }
    }
}

static U64 rook_naive(int sq, U64 occ) {
    U64 attacks = 0;
    int c = FILE(sq), r = RANK(sq);
    for (int nr = r + 1; nr < 8; nr++) { attacks |= SQUARES[SQUARE(c, nr)]; if (occ & SQUARES[SQUARE(c, nr)]) break; }
    for (int nr = r - 1; nr >= 0; nr--) { attacks |= SQUARES[SQUARE(c, nr)]; if (occ & SQUARES[SQUARE(c, nr)]) break; }
    for (int nc = c + 1; nc < 8; nc++) { attacks |= SQUARES[SQUARE(nc, r)]; if (occ & SQUARES[SQUARE(nc, r)]) break; }
    for (int nc = c - 1; nc >= 0; nc--) { attacks |= SQUARES[SQUARE(nc, r)]; if (occ & SQUARES[SQUARE(nc, r)]) break; }
    return attacks;
}

static U64 bishop_naive(int sq, U64 occ) {
    U64 attacks = 0;
    int c = FILE(sq), r = RANK(sq);
    for (int nc = c + 1, nr = r + 1; nc < 8 && nr < 8; nc++, nr++) { attacks |= SQUARES[SQUARE(nc, nr)]; if (occ & SQUARES[SQUARE(nc, nr)]) break; }
    for (int nc = c - 1, nr = r + 1; nc >= 0 && nr < 8; nc--, nr++) { attacks |= SQUARES[SQUARE(nc, nr)]; if (occ & SQUARES[SQUARE(nc, nr)]) break; }
    for (int nc = c + 1, nr = r - 1; nc < 8 && nr >= 0; nc++, nr--) { attacks |= SQUARES[SQUARE(nc, nr)]; if (occ & SQUARES[SQUARE(nc, nr)]) break; }
    for (int nc = c - 1, nr = r - 1; nc >= 0 && nr >= 0; nc--, nr--) { attacks |= SQUARES[SQUARE(nc, nr)]; if (occ & SQUARES[SQUARE(nc, nr)]) break; }
    return attacks;
}

//=============================================================================
// Sliding Attacks
//=============================================================================

U64 bb_rook_attacks(int sq, U64 occ) {
    U64 blockers = occ & ROOK_BLOCKER[sq];
    int idx = (int)((blockers * ROOK_MAGIC[sq]) >> ROOK_SHIFT[sq]);
    return ROOK_TABLE[(sq << 10) | idx];
}

U64 bb_bishop_attacks(int sq, U64 occ) {
    U64 blockers = occ & BISHOP_BLOCKER[sq];
    int idx = (int)((blockers * BISHOP_MAGIC[sq]) >> BISHOP_SHIFT[sq]);
    return BISHOP_TABLE[(sq << 6) | idx];
}

//=============================================================================
// Line / Between
//=============================================================================

U64 bb_line(int s1, int s2) {
    U64 l = 0;
    int dc = FILE(s2) - FILE(s1);
    int dr = RANK(s2) - RANK(s1);
    if (dc == 0 || dr == 0 || abs(dc) == abs(dr)) {
        int c = FILE(s1), r = RANK(s1);
        int nc = c, nr = r;
        while (1) {
            l |= SQUARES[SQUARE(nc, nr)];
            if (nc == FILE(s2) && nr == RANK(s2)) break;
            nc += (dc > 0) - (dc < 0);
            nr += (dr > 0) - (dr < 0);
        }
        l &= ~SQUARES[s1] & ~SQUARES[s2];
    }
    return l;
}

U64 bb_between(int s1, int s2) {
    U64 b = 0;
    int dc = FILE(s2) - FILE(s1);
    int dr = RANK(s2) - RANK(s1);
    if (dc == 0 || dr == 0 || abs(dc) == abs(dr)) {
        int c = FILE(s1) + ((dc > 0) ? 1 : (dc < 0) ? -1 : 0);
        int r = RANK(s1) + ((dr > 0) ? 1 : (dr < 0) ? -1 : 0);
        while (c >= 0 && c < 8 && r >= 0 && r < 8 && (c != FILE(s2) || r != RANK(s2))) {
            b |= SQUARES[SQUARE(c, r)];
            c += (dc > 0) ? 1 : (dc < 0) ? -1 : 0;
            r += (dr > 0) ? 1 : (dr < 0) ? -1 : 0;
        }
    }
    return b;
}
