#ifndef EVAL_H
#define EVAL_H

#include "types.h"

//=============================================================================
// PeSTO's Evaluation Function
//=============================================================================

// Initialize PST tables
void eval_init(void);

// Main evaluation
int evaluate(const Board *b);

// Tapered eval
int evaluate_tapered(const Board *b);

// Phase
int calc_phase(const Board *b);

//=============================================================================
// PeSTO Material Values
//=============================================================================

// Mg values indexed by piece type (1-6 = P,N,B,R,Q,K)
extern const int pesto_values_mg[7];
extern const int pesto_values_eg[7];

// PST tables: [piece][sq] = mg/eg bonus
// piece 1-6 for white (square as normal), 7-12 for black (square mirrored)
extern const int16_t PST_MG[13][64];
extern const int16_t PST_EG[13][64];

//=============================================================================
// Evaluation Terms
//=============================================================================

int material_score(const Board *b);
int pawn_score(const Board *b);
int king_safety(const Board *b, bool side);

// Bishop pair
int bishop_pair_score(const Board *b, bool side);

// Rook on open/semi-open
int rook_open_file(const Board *b, bool side, int sq);
int rook_semiopen(const Board *b, bool side, int sq);

// Passed pawns
int passed_pawn_score(const Board *b, bool side, int sq);

// Pawn weaknesses
int doubled_pawns(const Board *b, bool side);
int isolated_pawns(const Board *b, bool side);
int backward_pawns(const Board *b, bool side);

// Piece mobility
int mobility_score(const Board *b, bool side);

// Space
int space_score(const Board *b);

// Imbalances
int imbalance_score(const Board *b);

// Threat assessment
int threat_score(const Board *b, bool side);

// Complexity
int complexity_score(const Board *b);

//=============================================================================
// PST Accessors
//=============================================================================

static inline int pst_mg(int piece, int sq) {
    return PST_MG[piece][sq];
}

static inline int pst_eg(int piece, int sq) {
    return PST_EG[piece][sq];
}

static inline int pst_tapered(int piece, int sq, int phase) {
    int mg = PST_MG[piece][sq];
    int eg = PST_EG[piece][sq];
    return (mg * phase + eg * (24 - phase)) / 24;
}

#endif // EVAL_H
