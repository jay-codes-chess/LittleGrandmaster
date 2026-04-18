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
    mg += passed_pawns(b, WHITE) - passed_pawns(b, BLACK);

    // Mobility
    mg += mobility_score(b, WHITE) - mobility_score(b, BLACK);

    // Knight and bishop outposts
    {
        U64 w_knights = b->knights & b->white;
        while (w_knights) mg += knight_outpost_score(b, WHITE, pop_lsb(&w_knights));
        U64 b_knights = b->knights & b->black;
        while (b_knights) mg -= knight_outpost_score(b, BLACK, pop_lsb(&b_knights));
        U64 w_bishops = b->bishops & b->white;
        while (w_bishops) mg += bishop_outpost_score(b, WHITE, pop_lsb(&w_bishops));
        U64 b_bishops = b->bishops & b->black;
        while (b_bishops) mg -= bishop_outpost_score(b, BLACK, pop_lsb(&b_bishops));
    }

    // Rook on open/semi-open file
    {
        U64 w_rooks = b->rooks & b->white;
        while (w_rooks) mg += rook_open_file(b, WHITE, pop_lsb(&w_rooks));
        U64 b_rooks = b->rooks & b->black;
        while (b_rooks) mg -= rook_open_file(b, BLACK, pop_lsb(&b_rooks));
    }

    // King safety
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
    return 0;  // Stub - see passed_pawns() below
}

//=============================================================================
// Passed Pawns (full implementation)
//=============================================================================

int passed_pawns(const Board *b, bool side) {
    int score = 0;
    U64 pawns = b->pawns & (side ? b->white : b->black);
    while (pawns) {
        int sq = pop_lsb(&pawns);
        int r = RANK(sq);
        int f = FILE(sq);

        // Check if pawn is passed
        U64 enemy_pawns = b->pawns & (side ? b->black : b->white);
        bool passed = true;

        // For white pawns: check ranks r+1 to 7 in same file and adjacent files
        // For black pawns: check ranks 0 to r-1
        int start = side ? r + 1 : 0;
        int end   = side ? 8   : r;

        for (int check_r = start; check_r < end && passed; check_r++) {
            for (int check_f = max(0, f - 1); check_f <= min(7, f + 1) && passed; check_f++) {
                if (enemy_pawns & SQUARES[SQUARE(check_f, check_r)])
                    passed = false;
            }
        }

        if (passed) {
            // Scale by rank — further up = more dangerous
            int rank_idx = side ? r : (7 - r);
            score += PASSED_PAWN_MG[side][rank_idx];
        }
    }
    return score;
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
// Mobility
//=============================================================================

int mobility_score(const Board *b, bool side) {
    int mg = 0;
    U64 occ = b->white | b->black;

    // Knights
    U64 knights = b->knights & (side ? b->white : b->black);
    while (knights) {
        int sq = pop_lsb(&knights);
        U64 attacks = KNIGHT_ATTACKS[sq] & ~occ;
        int n = popcount(attacks);
        mg += MOBILITY_KNIGHT * (n - 4);  // baseline 4 attack squares
    }

    // Bishops
    U64 bishops = b->bishops & (side ? b->white : b->black);
    while (bishops) {
        int sq = pop_lsb(&bishops);
        U64 attacks = bb_bishop_attacks(sq, occ) & ~occ;
        int n = popcount(attacks);
        mg += MOBILITY_BISHOP * (n - 7);  // baseline 7 attack squares
    }

    // Rooks
    U64 rooks = b->rooks & (side ? b->white : b->black);
    while (rooks) {
        int sq = pop_lsb(&rooks);
        U64 attacks = bb_rook_attacks(sq, occ) & ~occ;
        int n = popcount(attacks);
        mg += MOBILITY_ROOK * (n - 7);  // baseline 7 attack squares
    }

    // Queens
    U64 queens = b->queens & (side ? b->white : b->black);
    while (queens) {
        int sq = pop_lsb(&queens);
        U64 attacks = (bb_bishop_attacks(sq, occ) | bb_rook_attacks(sq, occ)) & ~occ;
        int n = popcount(attacks);
        mg += MOBILITY_QUEEN * (n - 14);  // baseline 14 attack squares
    }

    return mg;
}

//=============================================================================
// Rook on Open / Semi-Open File
//=============================================================================

int rook_open_file(const Board *b, bool side, int sq) {
    int f = FILE(sq);
    U64 file_pawns = b->pawns & FILES[f];
    U64 own_pawns = file_pawns & (side ? b->white : b->black);
    U64 enemy_pawns = file_pawns & (side ? b->black : b->white);

    if (!own_pawns) {
        // Open file
        if (!enemy_pawns) return ROOK_OPEN_FILE;
        // Semi-open file
        return ROOK_SEMIOPEN;
    }
    return 0;
}

int rook_semiopen(const Board *b, bool side, int sq) {
    return rook_open_file(b, side, sq);  // simplified: same function handles both
}

//=============================================================================
// Outpost Score (Knights and Bishops)
//=============================================================================

int knight_outpost_score(const Board *b, bool side, int sq) {
    // A knight outpost is: protected by a pawn, can't be attacked by enemy pawns
    int dir = side ? 1 : -1;  // pawn advance direction for this side
    int prom_rank = side ? 7 : 0;

    // Check if protected by own pawn
    U64 protector = 0;
    int f = FILE(sq);
    int r = RANK(sq);
    if (f > 0 && r >= 1 && r <= 6)
        protector |= SQUARES[SQUARE(f - 1, r - dir)];
    if (f < 7 && r >= 1 && r <= 6)
        protector |= SQUARES[SQUARE(f + 1, r - dir)];

    U64 own_pawns = b->pawns & (side ? b->white : b->black);
    U64 enemy_pawns = b->pawns & (side ? b->black : b->white);
    bool protected_by_pawn = (protector & own_pawns) != 0;
    bool safe_from_pawns = ((KNIGHT_ATTACKS[sq] & enemy_pawns) == 0);

    if (!protected_by_pawn || !safe_from_pawns) return 0;

    // Bonus scales with rank — deeper into enemy territory = better
    int rank = side ? r : (7 - r);
    if (rank >= 4) return OUTPOST_MIN + (rank - 4) * 4;
    if (rank >= 3) return OUTPOST_MIN / 2;
    return 0;
}

int bishop_outpost_score(const Board *b, bool side, int sq) {
    int dir = side ? 1 : -1;
    int r = RANK(sq);
    int f = FILE(sq);

    U64 protector = 0;
    if (f > 0 && r >= 1 && r <= 6) protector |= SQUARES[SQUARE(f - 1, r - dir)];
    if (f < 7 && r >= 1 && r <= 6) protector |= SQUARES[SQUARE(f + 1, r - dir)];

    U64 own_pawns = b->pawns & (side ? b->white : b->black);
    U64 enemy_pawns = b->pawns & (side ? b->black : b->white);
    bool protected_by_pawn = (protector & own_pawns) != 0;
    bool safe_from_pawns = ((bb_bishop_attacks(sq, 0) & enemy_pawns) == 0);

    if (!protected_by_pawn || !safe_from_pawns) return 0;

    int rank = side ? r : (7 - r);
    if (rank >= 4) return OUTPOST_MIN + (rank - 4) * 4;
    if (rank >= 3) return OUTPOST_MIN / 2;
    return 0;
}

//=============================================================================
// King Safety
//=============================================================================

int king_safety(const Board *b, bool side) {
    int score = 0;
    int king_sq = ctz(b->kings & (side ? b->white : b->black));
    int enemy = side ^ 1;

    // King zone: squares the king attacks + one rank/file in front
    U64 king_zone = KING_ATTACKS[king_sq];
    if (side == WHITE) king_zone |= ShiftSouth(king_zone);
    else               king_zone |= ShiftNorth(king_zone);

    // Count enemy attacks on king zone by pieces (not pawns)
    U64 enemy_knights = b->knights & (enemy ? b->white : b->black);
    U64 enemy_bishops = b->bishops & (enemy ? b->white : b->black);
    U64 enemy_rooks   = b->rooks   & (enemy ? b->white : b->black);
    U64 enemy_queens  = b->queens  & (enemy ? b->white : b->black);

    int attacks = 0;
    while (enemy_knights) attacks += (KNIGHT_ATTACKS[pop_lsb(&enemy_knights)] & king_zone) != 0;
    while (enemy_bishops) attacks += (bb_bishop_attacks(pop_lsb(&enemy_bishops), 0) & king_zone) != 0;
    while (enemy_rooks)   attacks += (bb_rook_attacks(pop_lsb(&enemy_rooks), 0) & king_zone) != 0;
    while (enemy_queens)  attacks += ((bb_bishop_attacks(pop_lsb(&enemy_queens), 0) | bb_rook_attacks(pop_lsb(&enemy_queens), 0)) & king_zone) != 0;

    // Pawn shelter: friendly pawns in front of king
    int dir = side ? 1 : -1;
    U64 own_pawns = b->pawns & (side ? b->white : b->black);
    int shelter = 0;
    for (int df = -1; df <= 1; df++) {
        int f = FILE(king_sq) + df;
        if (f < 0 || f > 7) continue;
        int pawn_sq = SQUARE(f, RANK(king_sq) + dir);
        if (pawn_sq >= 0 && pawn_sq < 64 && (own_pawns & SQUARES[pawn_sq])) {
            shelter += 5;
        }
    }

    // Bonus if queen is present to capitalize on attacks
    U64 own_queen = b->queens & (side ? b->white : b->black);
    if (own_queen) score -= attacks * KING_ATTACK_WEIGHT;  // enemy attacks on our king = bad
    score += shelter;

    return -attacks * KING_ATTACK_WEIGHT + shelter;
}
