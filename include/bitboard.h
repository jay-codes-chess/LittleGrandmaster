#ifndef BITBOARD_H
#define BITBOARD_H

#include "types.h"

//=============================================================================
// Precomputed Tables
//=============================================================================

extern U64 SQUARES[64];
extern U64 FILES[8];
extern U64 RANKS[8];
extern U64 KNIGHT_SPAN[64];
extern U64 KING_SPAN[64];
extern U64 PAWN_ATTACKS_W[64];
extern U64 PAWN_ATTACKS_B[64];
extern U64 KNIGHT_ATTACKS[64];
extern U64 KING_ATTACKS[64];

// Shift operations
static inline U64 ShiftNorth(U64 b) { return b << 8; }
static inline U64 ShiftSouth(U64 b) { return b >> 8; }
extern U64 CENTER;
extern U64 BIG_CENTER;

//=============================================================================
// Initialization
//=============================================================================

void bb_init(void);

//=============================================================================
// Sliding Attacks (magic bitboard lookup)
//=============================================================================

U64 bb_rook_attacks(int sq, U64 occ);
U64 bb_bishop_attacks(int sq, U64 occ);

static inline U64 bb_queen_attacks(int sq, U64 occ) {
    return bb_rook_attacks(sq, occ) | bb_bishop_attacks(sq, occ);
}

//=============================================================================
// Line / Between
//=============================================================================

U64 bb_line(int sq1, int sq2);
U64 bb_between(int sq1, int sq2);

//=============================================================================
// Bitboard Helpers
//=============================================================================

static inline int lsb(U64 b) { return __builtin_ctzll(b); }
static inline int msb(U64 b) { return 63 - __builtin_clzll(b); }
static inline int pop_lsb(U64 *b) { int s = __builtin_ctzll(*b); *b &= *b - 1; return s; }

#endif // BITBOARD_H
