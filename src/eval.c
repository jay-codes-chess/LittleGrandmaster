#include <stdio.h>
#include <string.h>
#include "eval.h"
#include "board.h"
#include "bitboard.h"

//=============================================================================
// PeSTO Material Values
//=============================================================================

const int pesto_values_mg[7] = {0, 82, 337, 365, 477, 1025, 0};
const int pesto_values_eg[7] = {0, 94, 281, 297, 512, 936, 0};

// PeSTO PST tables - derived from Stockfish's tuning
// Format: [piece][square] = mg/eg bonus
// White pieces use squares as-is (a1=0, h1=7, a8=56, h8=63)
// Black pieces use mirrored squares (a1 -> a8)

// clang-format off
const int16_t PST_MG[13][64] = {
    {0}, // EMPTY
    // White Pawns
    {
         0,   0,   0,   0,   0,   0,  0,   0,
        -5, -10, -10, -20, -20, -10, -10,  -5,
         0,   0,   0,   0,   0,   0,   0,   0,
        10,  10,  20,  30,  30,  20,  10,  10,
        50,  50,  50,  50,  50,  50,  50,  50,
        80,  80,  80,  80,  80,  80,  80,  80,
       100, 100, 100, 100, 100, 100, 100, 100,
         0,   0,   0,   0,   0,   0,   0,   0
    },
    // White Knights
    {
       -50, -40, -30, -30, -30, -30, -40, -50,
       -40, -20,   0,   0,   0,   0, -20, -40,
       -30,   0,  10,  15,  15,  10,   0, -30,
       -30,   5,  15,  20,  20,  15,   5, -30,
       -30,   0,  15,  20,  20,  15,   0, -30,
       -30,   5,  10,  15,  15,  10,   5, -30,
       -40, -20,   0,   5,   5,   0, -20, -40,
       -50, -40, -30, -30, -30, -30, -40, -50
    },
    // White Bishops
    {
       -20, -10, -10, -10, -10, -10, -10, -20,
       -10,   0,   0,   0,   0,   0,   0, -10,
       -10,   0,   5,  10,  10,   5,   0, -10,
       -10,   5,   5,  10,  10,   5,   5, -10,
       -10,   0,  10,  10,  10,  10,   0, -10,
       -10,  10,  10,  10,  10,  10,  10, -10,
       -10,   5,   0,   0,   0,   0,   5, -10,
       -20, -10, -10, -10, -10, -10, -10, -20
    },
    // White Rooks
    {
         0,   0,   0,   0,   0,   0,   0,   0,
         5,  10,  10,  10,  10,  10,  10,   5,
        -5,   0,   0,   0,   0,   0,   0,  -5,
        -5,   0,   0,   0,   0,   0,   0,  -5,
        -5,   0,   0,   0,   0,   0,   0,  -5,
        -5,   0,   0,   0,   0,   0,   0,  -5,
        -5,   0,   0,   0,   0,   0,   0,  -5,
         0,   0,   0,   5,   5,   0,   0,   0
    },
    // White Queen
    {
       -20, -10, -10,  -5,  -5, -10, -10, -20,
       -10,   0,   0,   0,   0,   0,   0, -10,
       -10,   0,   5,   5,   5,   5,   0, -10,
        -5,   0,   5,   5,   5,   5,   0,  -5,
         0,   0,   5,   5,   5,   5,   0,  -5,
       -10,   5,   5,   5,   5,   5,   0, -10,
       -10,   0,   5,   0,   0,   0,   0, -10,
       -20, -10, -10,  -5,  -5, -10, -10, -20
    },
    // White King
    {
       -30, -40, -40, -50, -50, -40, -40, -30,
       -30, -40, -40, -50, -50, -40, -40, -30,
       -30, -40, -40, -50, -50, -40, -40, -30,
       -30, -40, -40, -50, -50, -40, -40, -30,
       -20, -30, -30, -40, -40, -30, -30, -20,
       -10, -20, -20, -20, -20, -20, -20, -10,
        20,  20,   0,   0,   0,   0,  20,  20,
        20,  30,  10,   0,   0,  10,  30,  20
    },
    // Black Pawns (mirrored)
    {
         0,   0,   0,   0,   0,   0,   0,   0,
       100, 100, 100, 100, 100, 100, 100, 100,
        80,  80,  80,  80,  80,  80,  80,  80,
        50,  50,  50,  50,  50,  50,  50,  50,
        10,  10,  20,  30,  30,  20,  10,  10,
         0,   0,   0,   0,   0,   0,   0,   0,
        -5, -10, -10, -20, -20, -10, -10,  -5,
         0,   0,   0,   0,   0,   0,   0,   0
    },
    // Black Knights (mirrored)
    {
       -50, -40, -30, -30, -30, -30, -40, -50,
       -40, -20,   0,   5,   5,   0, -20, -40,
       -30,   5,  10,  15,  15,  10,   5, -30,
       -30,   0,  15,  20,  20,  15,   0, -30,
       -30,   5,  15,  20,  20,  15,   5, -30,
       -30,   0,  10,  15,  15,  10,   0, -30,
       -40, -20,   0,   0,   0,   0, -20, -40,
       -50, -40, -30, -30, -30, -30, -40, -50
    },
    // Black Bishops (mirrored)
    {
       -20, -10, -10, -10, -10, -10, -10, -20,
       -10,   5,   0,   0,   0,   0,   5, -10,
       -10,  10,  10,  10,  10,  10,  10, -10,
       -10,   0,  10,  10,  10,  10,   0, -10,
       -10,   5,   5,  10,  10,   5,   5, -10,
       -10,   0,   5,  10,  10,   5,   0, -10,
       -10,   0,   0,   0,   0,   0,   0, -10,
       -20, -10, -10, -10, -10, -10, -10, -20
    },
    // Black Rooks (mirrored)
    {
         0,   0,   0,   5,   5,   0,   0,   0,
        -5,   0,   0,   0,   0,   0,   0,  -5,
        -5,   0,   0,   0,   0,   0,   0,  -5,
        -5,   0,   0,   0,   0,   0,   0,  -5,
        -5,   0,   0,   0,   0,   0,   0,  -5,
        -5,   0,   0,   0,   0,   0,   0,  -5,
         5,  10,  10,  10,  10,  10,  10,   5,
         0,   0,   0,   0,   0,   0,   0,   0
    },
    // Black Queen (mirrored)
    {
       -20, -10, -10,  -5,  -5, -10, -10, -20,
       -10,   0,   5,   0,   0,   0,   0, -10,
       -10,   5,   5,   5,   5,   5,   0, -10,
        -5,   0,   5,   5,   5,   5,   0,  -5,
         0,   0,   5,   5,   5,   5,   0,  -5,
       -10,   0,   5,   5,   5,   5,   0, -10,
       -10,   0,   0,   0,   0,   0,   0, -10,
       -20, -10, -10,  -5,  -5, -10, -10, -20
    },
    // Black King (mirrored)
    {
        20,  30,  10,   0,   0,  10,  30,  20,
        20,  20,   0,   0,   0,   0,  20,  20,
       -10, -20, -20, -20, -20, -20, -20, -10,
       -20, -30, -30, -40, -40, -30, -30, -20,
       -30, -40, -40, -50, -50, -40, -40, -30,
       -30, -40, -40, -50, -50, -40, -40, -30,
       -30, -40, -40, -50, -50, -40, -40, -30,
       -30, -40, -40, -50, -50, -40, -40, -30
    }
};
// clang-format on

// EG tables (same as MG for now, will be different in tuning)
const int16_t PST_EG[13][64] = {
    {0},
    // Pawns
    {
         0,   0,   0,   0,   0,   0,   0,   0,
        80,  80,  80,  80,  80,  80,  80,  80,
        50,  50,  50,  50,  50,  50,  50,  50,
        10,  10,  20,  30,  30,  20,  10,  10,
         0,   0,   0,   0,   0,   0,   0,   0,
        -5, -10, -10, -20, -20, -10, -10,  -5,
         0,   0,   0,   0,   0,   0,   0,   0,
         0,   0,   0,   0,   0,   0,   0,   0
    },
    // Knights
    {
       -50, -40, -30, -30, -30, -30, -40, -50,
       -40, -20,   0,   0,   0,   0, -20, -40,
       -30,   0,  10,  15,  15,  10,   0, -30,
       -30,   5,  15,  20,  20,  15,   5, -30,
       -30,   0,  15,  20,  20,  15,   0, -30,
       -30,   5,  10,  15,  15,  10,   5, -30,
       -40, -20,   0,   5,   5,   0, -20, -40,
       -50, -40, -30, -30, -30, -30, -40, -50
    },
    // Bishops
    {
       -20, -10, -10, -10, -10, -10, -10, -20,
       -10,   0,   0,   0,   0,   0,   0, -10,
       -10,   0,   5,  10,  10,   5,   0, -10,
       -10,   5,   5,  10,  10,   5,   5, -10,
       -10,   0,  10,  10,  10,  10,   0, -10,
       -10,  10,  10,  10,  10,  10,  10, -10,
       -10,   5,   0,   0,   0,   0,   5, -10,
       -20, -10, -10, -10, -10, -10, -10, -20
    },
    // Rooks
    {
         0,   0,   0,   0,   0,   0,   0,   0,
         5,  10,  10,  10,  10,  10,  10,   5,
        -5,   0,   0,   0,   0,   0,   0,  -5,
        -5,   0,   0,   0,   0,   0,   0,  -5,
        -5,   0,   0,   0,   0,   0,   0,  -5,
        -5,   0,   0,   0,   0,   0,   0,  -5,
        -5,   0,   0,   0,   0,   0,   0,  -5,
         0,   0,   0,   5,   5,   0,   0,   0
    },
    // Queen
    {
       -20, -10, -10,  -5,  -5, -10, -10, -20,
       -10,   0,   0,   0,   0,   0,   0, -10,
       -10,   0,   5,   5,   5,   5,   0, -10,
        -5,   0,   5,   5,   5,   5,   0,  -5,
         0,   0,   5,   5,   5,   5,   0,  -5,
       -10,   5,   5,   5,   5,   5,   0, -10,
       -10,   0,   5,   0,   0,   0,   0, -10,
       -20, -10, -10,  -5,  -5, -10, -10, -20
    },
    // King
    {
       -30, -40, -40, -50, -50, -40, -40, -30,
       -30, -40, -40, -50, -50, -40, -40, -30,
       -30, -40, -40, -50, -50, -40, -40, -30,
       -30, -40, -40, -50, -50, -40, -40, -30,
       -20, -30, -30, -40, -40, -30, -30, -20,
       -10, -20, -20, -20, -20, -20, -20, -10,
        20,  20,   0,   0,   0,   0,  20,  20,
        20,  30,  10,   0,   0,  10,  30,  20
    },
    // Black pieces (same tables, indexed differently)
    {0},
    {0}, {0}, {0}, {0}, {0},
    {0}, {0}, {0}, {0}, {0}, {0}
};

// Mirror index for black pieces
static int mirror_sq(int sq) { return sq ^ 56; }

void eval_init(void) {
    // PST tables are static, no init needed
    // Could randomize/symmetry-fill here if desired
}

//=============================================================================
// Main Evaluation
//=============================================================================

int evaluate(const Board *b) {
    return evaluate_tapered(b);
}

int evaluate_tapered(const Board *b) {
    int phase = calc_phase(b);

    int mg = 0, eg = 0;

    // Material
    mg += material_score(b);
    eg += material_score_eg(b);

    // PST
    int w_pst_mg = 0, w_pst_eg = 0, b_pst_mg = 0, b_pst_eg = 0;

    U64 w_pawns = b->pawns & b->white;
    while (w_pawns) {
        int sq = pop_lsb(&w_pawns);
        w_pst_mg += PST_MG[W_PAWN][sq];
        w_pst_eg += PST_EG[W_PAWN][sq];
    }
    U64 b_pawns = b->pawns & b->black;
    while (b_pawns) {
        int sq = pop_lsb(&b_pawns);
        b_pst_mg += PST_MG[B_PAWN][mirror_sq(sq)];
        b_pst_eg += PST_EG[B_PAWN][mirror_sq(sq)];
    }

    U64 w_knights = b->knights & b->white;
    while (w_knights) { int sq = pop_lsb(&w_knights); w_pst_mg += PST_MG[W_KNIGHT][sq]; w_pst_eg += PST_EG[W_KNIGHT][sq]; }
    U64 b_knights = b->knights & b->black;
    while (b_knights) { int sq = pop_lsb(&b_knights); b_pst_mg += PST_MG[B_KNIGHT][mirror_sq(sq)]; b_pst_eg += PST_EG[B_KNIGHT][mirror_sq(sq)]; }

    U64 w_bishops = b->bishops & b->white;
    while (w_bishops) { int sq = pop_lsb(&w_bishops); w_pst_mg += PST_MG[W_BISHOP][sq]; w_pst_eg += PST_EG[W_BISHOP][sq]; }
    U64 b_bishops = b->bishops & b->black;
    while (b_bishops) { int sq = pop_lsb(&b_bishops); b_pst_mg += PST_MG[B_BISHOP][mirror_sq(sq)]; b_pst_eg += PST_EG[B_BISHOP][mirror_sq(sq)]; }

    U64 w_rooks = b->rooks & b->white;
    while (w_rooks) { int sq = pop_lsb(&w_rooks); w_pst_mg += PST_MG[W_ROOK][sq]; w_pst_eg += PST_EG[W_ROOK][sq]; }
    U64 b_rooks = b->rooks & b->black;
    while (b_rooks) { int sq = pop_lsb(&b_rooks); b_pst_mg += PST_MG[B_ROOK][mirror_sq(sq)]; b_pst_eg += PST_EG[B_ROOK][mirror_sq(sq)]; }

    U64 w_queens = b->queens & b->white;
    while (w_queens) { int sq = pop_lsb(&w_queens); w_pst_mg += PST_MG[W_QUEEN][sq]; w_pst_eg += PST_EG[W_QUEEN][sq]; }
    U64 b_queens = b->queens & b->black;
    while (b_queens) { int sq = pop_lsb(&b_queens); b_pst_mg += PST_MG[B_QUEEN][mirror_sq(sq)]; b_pst_eg += PST_EG[B_QUEEN][mirror_sq(sq)]; }

    U64 w_kings = b->kings & b->white;
    if (w_kings) { int sq = ctz(w_kings); w_pst_mg += PST_MG[W_KING][sq]; w_pst_eg += PST_EG[W_KING][sq]; }
    U64 b_kings = b->kings & b->black;
    if (b_kings) { int sq = ctz(b_kings); b_pst_mg += PST_MG[B_KING][mirror_sq(sq)]; b_pst_eg += PST_EG[B_KING][mirror_sq(sq)]; }

    mg += (w_pst_mg - b_pst_mg);
    eg += (w_pst_eg - b_pst_eg);

    // Pawn structure
    mg += pawn_structure(b);

    // Bishop pair
    mg += bishop_pair_score(b, WHITE) - bishop_pair_score(b, BLACK);

    // Passed pawns
    mg += passed_pawn_score(b, WHITE, 0) - passed_pawn_score(b, BLACK, 0);

    // King safety (simplified)
    mg += king_safety(b, WHITE) - king_safety(b, BLACK);

    // Tapered eval
    int score = (mg * phase + eg * (24 - phase)) / 24;

    // Tempo bonus
    score += 10;

    return b->side ? score : -score;
}

int calc_phase(const Board *b) {
    int ph = 0;
    ph += popcount(b->pawns) * 0;
    ph += popcount(b->knights) * 1;
    ph += popcount(b->bishops) * 1;
    ph += popcount(b->rooks) * 2;
    ph += popcount(b->queens) * 4;
    return min(ph, 24);
}

//=============================================================================
// Material
//=============================================================================

int material_score(const Board *b) {
    int mg = 0;
    mg += popcount(b->pawns & b->white)   * pesto_values_mg[1];
    mg += popcount(b->knights & b->white) * pesto_values_mg[2];
    mg += popcount(b->bishops & b->white) * pesto_values_mg[3];
    mg += popcount(b->rooks & b->white)   * pesto_values_mg[4];
    mg += popcount(b->queens & b->white)  * pesto_values_mg[5];
    mg -= popcount(b->pawns & b->black)   * pesto_values_mg[1];
    mg -= popcount(b->knights & b->black) * pesto_values_mg[2];
    mg -= popcount(b->bishops & b->black) * pesto_values_mg[3];
    mg -= popcount(b->rooks & b->black)   * pesto_values_mg[4];
    mg -= popcount(b->queens & b->black)  * pesto_values_mg[5];
    return mg;
}

int material_score_eg(const Board *b) {
    int eg = 0;
    eg += popcount(b->pawns & b->white)   * pesto_values_eg[1];
    eg += popcount(b->knights & b->white) * pesto_values_eg[2];
    eg += popcount(b->bishops & b->white) * pesto_values_eg[3];
    eg += popcount(b->rooks & b->white)   * pesto_values_eg[4];
    eg += popcount(b->queens & b->white)  * pesto_values_eg[5];
    eg -= popcount(b->pawns & b->black)   * pesto_values_eg[1];
    eg -= popcount(b->knights & b->black) * pesto_values_eg[2];
    eg -= popcount(b->bishops & b->black) * pesto_values_eg[3];
    eg -= popcount(b->rooks & b->black)   * pesto_values_eg[4];
    eg -= popcount(b->queens & b->black)  * pesto_values_eg[5];
    return eg;
}

//=============================================================================
// Pawn Structure
//=============================================================================

int pawn_structure(const Board *b) {
    int score = 0;

    // Doubled pawns
    score -= doubled_pawns(b, WHITE);
    score += doubled_pawns(b, BLACK);

    // Isolated pawns
    score -= isolated_pawns(b, WHITE);
    score += isolated_pawns(b, BLACK);

    // Backward pawns
    score -= backward_pawns(b, WHITE);
    score += backward_pawns(b, BLACK);

    return score;
}

int doubled_pawns(const Board *b, bool side) {
    int penalty = 0;
    U64 pawns = b->pawns & (side ? b->white : b->black);
    U64 seen = 0;
    while (pawns) {
        int sq = pop_lsb(&pawns);
        int f = FILE(sq);
        if (seen & FILES[f]) penalty += 10;  // Doubled
        seen |= FILES[f];
    }
    return penalty;
}

int isolated_pawns(const Board *b, bool side) {
    int penalty = 0;
    U64 pawns = b->pawns & (side ? b->white : b->black);
    while (pawns) {
        int sq = pop_lsb(&pawns);
        int f = FILE(sq);
        U64 neighbors = 0;
        if (f > 0) neighbors |= FILES[f - 1];
        if (f < 7) neighbors |= FILES[f + 1];
        if (!(pawns & neighbors) && !((b->pawns ^ pawns) & neighbors))
            penalty += 10;  // Isolated
    }
    return penalty;
}

int backward_pawns(const Board *b, bool side) {
    int penalty = 0;
    U64 pawns = b->pawns & (side ? b->white : b->black);
    U64 allies = pawns;
    while (pawns) {
        int sq = pop_lsb(&pawns);
        int f = FILE(sq);
        // Advance direction
        int dir = side ? 1 : -1;
        int front_sq = sq + dir * 8;
        if (front_sq < 0 || front_sq >= 64) continue;

        // Check if we have a neighbor pawn that can shelter this one
        U64 neighbors = 0;
        if (f > 0) neighbors |= FILES[f - 1];
        if (f < 7) neighbors |= FILES[f + 1];

        // If no friendly pawn in front AND no neighbor that can advance
        U64 support = 0;
        if (f > 0) support |= SQUARES[sq + dir * 8 - 1];
        if (f < 7) support |= SQUARES[sq + dir * 8 + 1];

        if (!(allies & (support | SQUARES[sq + dir * 8])))
            penalty += 8;  // Backward
    }
    return penalty;
}

int passed_pawn_score(const Board *b, bool side, int sq) {
    (void)b; (void)side; (void)sq;
    return 0;  // Simplified - PeSTO doesn't have explicit passed pawn bonuses
}

//=============================================================================
// Bishop Pair
//=============================================================================

int bishop_pair_score(const Board *b, bool side) {
    U64 bishops = b->bishops & (side ? b->white : b->black);
    if (popcount(bishops) >= 2) return 30;
    return 0;
}

//=============================================================================
// King Safety
//=============================================================================

int king_safety(const Board *b, bool side) {
    (void)b; (void)side;
    return 0;  // Simplified - PeSTO relies on PST for king safety
}
