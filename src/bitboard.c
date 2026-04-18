//=============================================================================
// LittleGrandmaster Bitboard System
//=============================================================================

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
U64 KNIGHT_ATTACKS[64];
U64 KING_ATTACKS[64];
U64 PAWN_ATTACKS_W[64];
U64 PAWN_ATTACKS_B[64];
U64 CENTER;
U64 BIG_CENTER;

//=============================================================================
// Bit Operations
//=============================================================================

int bb_popcount(U64 b) {
    return __builtin_popcountll(b);
}

int bb_first_one(U64 b) {
    return __builtin_ctzll(b);
}

//=============================================================================
// Sliding Attacks - Naive (Reliable)
//=============================================================================

U64 bb_rook_attacks(int sq, U64 occ) {
    U64 attacks = 0;
    int c = FILE(sq), r = RANK(sq);
    
    // Positive rank direction (+rank, same file)
    for (int nr = r + 1; nr < 8; nr++) {
        int t = SQUARE(c, nr);
        attacks |= SQUARES[t];
        if (occ & SQUARES[t]) break;
    }
    // Negative rank direction (-rank, same file)
    for (int nr = r - 1; nr >= 0; nr--) {
        int t = SQUARE(c, nr);
        attacks |= SQUARES[t];
        if (occ & SQUARES[t]) break;
    }
    // Positive file direction (same rank, +file)
    for (int nc = c + 1; nc < 8; nc++) {
        int t = SQUARE(nc, r);
        attacks |= SQUARES[t];
        if (occ & SQUARES[t]) break;
    }
    // Negative file direction (same rank, -file)
    for (int nc = c - 1; nc >= 0; nc--) {
        int t = SQUARE(nc, r);
        attacks |= SQUARES[t];
        if (occ & SQUARES[t]) break;
    }
    
    return attacks;
}

U64 bb_bishop_attacks(int sq, U64 occ) {
    U64 attacks = 0;
    int c = FILE(sq), r = RANK(sq);
    
    // NE diagonal
    for (int nc = c + 1, nr = r + 1; nc < 8 && nr < 8; nc++, nr++) {
        int t = SQUARE(nc, nr);
        attacks |= SQUARES[t];
        if (occ & SQUARES[t]) break;
    }
    // NW diagonal
    for (int nc = c - 1, nr = r + 1; nc >= 0 && nr < 8; nc--, nr++) {
        int t = SQUARE(nc, nr);
        attacks |= SQUARES[t];
        if (occ & SQUARES[t]) break;
    }
    // SE diagonal
    for (int nc = c + 1, nr = r - 1; nc < 8 && nr >= 0; nc++, nr--) {
        int t = SQUARE(nc, nr);
        attacks |= SQUARES[t];
        if (occ & SQUARES[t]) break;
    }
    // SW diagonal
    for (int nc = c - 1, nr = r - 1; nc >= 0 && nr >= 0; nc--, nr--) {
        int t = SQUARE(nc, nr);
        attacks |= SQUARES[t];
        if (occ & SQUARES[t]) break;
    }
    
    return attacks;
}

//=============================================================================
// Initialization
//=============================================================================

void bb_init(void) {
    // SQUARES - bit 0 = A1, bit 1 = B1, etc.
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

    // Knight and King attack tables (same as SPAN, non-sliding)
    for (int sq = 0; sq < 64; sq++) {
        KNIGHT_ATTACKS[sq] = KNIGHT_SPAN[sq];
        KING_ATTACKS[sq]   = KING_SPAN[sq];
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
