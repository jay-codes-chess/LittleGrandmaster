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

// Mobility bonuses (per available square beyond baseline)
#define MOBILITY_KNIGHT  4
#define MOBILITY_BISHOP  5
#define MOBILITY_ROOK    3
#define MOBILITY_QUEEN   2

// Passed pawn bonuses by rank [rank 0-7]
static const int PASSED_PAWN_MG[2][8] = {
    { 0, 12, 12, 30, 50, 80, 130, 0 },  // white (ranks 0-7 from white's perspective)
    { 0, 120, 80, 50, 30, 12, 12, 0 }   // black
};
static const int PASSED_PAWN_EG[2][8] = {
    { 0, 16, 16, 39, 65, 104, 156, 0 },
    { 0, 156, 104, 65, 39, 16, 16, 0 }
};

// Rook on open file / semi-open file
#define ROOK_OPEN_FILE   12
#define ROOK_SEMIOPEN     6

// Outpost bonuses
#define OUTPOST_MIN      8
#define OUTPOST_MAX     24

// King safety
#define KING_ATTACK_WEIGHT  5  // weight per attack on king zone
#define KING_SHELTER_BASE  20  // base bonus for pawn shield

int material_score(const Board *b);
int material_score_eg(const Board *b);
int pawn_score(const Board *b);
int pawn_structure(const Board *b);
int king_safety(const Board *b, bool side);

// Bishop pair
int bishop_pair_score(const Board *b, bool side);

// Rook on open/semi-open
int rook_open_file(const Board *b, bool side, int sq);
int rook_semiopen(const Board *b, bool side, int sq);

// Passed pawns
int passed_pawn_score(const Board *b, bool side, int sq);
int passed_pawns(const Board *b, bool side);  // sum over all passed pawns

// Outposts
int knight_outpost_score(const Board *b, bool side, int sq);
int bishop_outpost_score(const Board *b, bool side, int sq);

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
