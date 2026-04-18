//=============================================================================
// Texel Tuner — Gradient Descent on Evaluation Parameters
// Peter McKenzie method: minimize cross-entropy error over training positions
//=============================================================================
//
// Usage: load .epd/.pgn file with positions + game results
// Format: FEN "result"  (result = "1-0", "0-1", "1/2-1/2")
//
// Each position: eval s → win prob q = 1/(1+10^(-s/400))
// Error: E = -sum(z*ln(q) + (1-z)*ln(1-q))
// Gradient: ∂E/∂p = sum(2(q-z) * dq/ds * ds/dp)
//
// SGD with learning rate and L2 regularization.
//=============================================================================

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include "bitboard.h"
#include "board.h"
#include "moves.h"
#include "eval.h"

//=============================================================================
// Tunable Parameters
//=============================================================================
typedef struct {
    float mobN, mobB, mobR, mobQ;   // mobility per extra square
    float p_doubled, p_isolated, p_backward;
    float outpost_min, outpost_per_rank;
    float rook_open, rook_semiopen;
    float bishop_pair;
    float tempo;
    float passed_mg[2][8];
    float passed_eg[2][8];
} Params;

static Params P;         // current params
static Params active;    // 1.0 = tune this param, 0.0 = fixed
static Params grad;      // accumulated gradients
static Params lr_scales; // per-param learning rate multipliers

//=============================================================================
// Training Data
//=============================================================================
#define MAX_POS 200000
static char  g_fen[MAX_POS][256];
static float g_result[MAX_POS];   // 1=white win, 0.5=draw, 0=black win
static int   g_score[MAX_POS];    // eval from white's POV (centipawns * SCALE)
static int   g_count = 0;
static int   g_scale = 1;         // eval scaling factor for sigmoid

//=============================================================================
// sigmoid: centipawns → white win probability
//=============================================================================
static double sigmoid(double s) {
    return 1.0 / (1.0 + pow(10.0, -s / 400.0));
}

//=============================================================================
// Re-evaluate all positions with current params
//=============================================================================
static double eval_position(const Board *b) {
    int phase = 0;
    phase += popcount(b->pawns) * 0;
    phase += popcount(b->knights) * 1;
    phase += popcount(b->bishops) * 1;
    phase += popcount(b->rooks) * 2;
    phase += popcount(b->queens) * 4;
    if (phase > 24) phase = 24;

    double mg = 0, eg = 0;

    // Material (PeSTO)
    mg += popcount(b->pawns   & b->white)   * 82.0;
    mg += popcount(b->knights & b->white)   * 337.0;
    mg += popcount(b->bishops & b->white)   * 365.0;
    mg += popcount(b->rooks   & b->white)   * 477.0;
    mg += popcount(b->queens  & b->white)   * 1025.0;
    mg -= popcount(b->pawns   & b->black)   * 82.0;
    mg -= popcount(b->knights & b->black)   * 337.0;
    mg -= popcount(b->bishops & b->black)   * 365.0;
    mg -= popcount(b->rooks   & b->black)   * 477.0;
    mg -= popcount(b->queens  & b->black)   * 1025.0;

    eg += popcount(b->pawns   & b->white)   * 94.0;
    eg += popcount(b->knights & b->white)   * 281.0;
    eg += popcount(b->bishops & b->white)   * 297.0;
    eg += popcount(b->rooks   & b->white)   * 512.0;
    eg += popcount(b->queens  & b->white)   * 936.0;
    eg -= popcount(b->pawns   & b->black)   * 94.0;
    eg -= popcount(b->knights & b->black)   * 281.0;
    eg -= popcount(b->bishops & b->black)   * 297.0;
    eg -= popcount(b->rooks   & b->black)   * 512.0;
    eg -= popcount(b->queens  & b->black)   * 936.0;

    // PST
    U64 w = b->pawns & b->white;  while (w) { int s = ctz(w); w &= w-1; mg += PST_MG[W_PAWN][s];   eg += PST_EG[W_PAWN][s]; }
    w = b->pawns & b->black;      while (w) { int s = ctz(w); w &= w-1; mg += PST_MG[B_PAWN][s];   eg += PST_EG[B_PAWN][s]; }
    w = b->knights & b->white;    while (w) { int s = ctz(w); w &= w-1; mg += PST_MG[W_KNIGHT][s]; eg += PST_EG[W_KNIGHT][s]; }
    w = b->knights & b->black;    while (w) { int s = ctz(w); w &= w-1; mg += PST_MG[B_KNIGHT][s]; eg += PST_EG[B_KNIGHT][s]; }
    w = b->bishops & b->white;    while (w) { int s = ctz(w); w &= w-1; mg += PST_MG[W_BISHOP][s]; eg += PST_EG[W_BISHOP][s]; }
    w = b->bishops & b->black;   while (w) { int s = ctz(w); w &= w-1; mg += PST_MG[B_BISHOP][s]; eg += PST_EG[B_BISHOP][s]; }
    w = b->rooks & b->white;     while (w) { int s = ctz(w); w &= w-1; mg += PST_MG[W_ROOK][s];   eg += PST_EG[W_ROOK][s]; }
    w = b->rooks & b->black;     while (w) { int s = ctz(w); w &= w-1; mg += PST_MG[B_ROOK][s];   eg += PST_EG[B_ROOK][s]; }
    w = b->queens & b->white;    while (w) { int s = ctz(w); w &= w-1; mg += PST_MG[W_QUEEN][s];  eg += PST_EG[W_QUEEN][s]; }
    w = b->queens & b->black;    while (w) { int s = ctz(w); w &= w-1; mg += PST_MG[B_QUEEN][s];  eg += PST_EG[B_QUEEN][s]; }
    w = b->kings & b->white;    if (w) { int s = ctz(w); mg += PST_MG[W_KING][s]; eg += PST_EG[W_KING][s]; }
    w = b->kings & b->black;    if (w) { int s = ctz(w); mg += PST_MG[B_KING][s]; eg += PST_EG[B_KING][s]; }

    // ---- TUNABLE TERMS ----

    // Pawn structure
    {
        U64 pawns = b->pawns & b->white;
        U64 seen = 0;
        while (pawns) {
            int sq = ctz(pawns); pawns &= pawns - 1;
            int f = FILE(sq);
            if (seen & FILES[f]) mg -= P.p_doubled;
            seen |= FILES[f];
        }
        pawns = b->pawns & b->white;
        while (pawns) {
            int sq = ctz(pawns); pawns &= pawns - 1;
            int f = FILE(sq);
            U64 nb = (f > 0 ? FILES[f-1] : 0) | (f < 7 ? FILES[f+1] : 0);
            if (!(b->pawns & nb)) mg -= P.p_isolated;
        }
        pawns = b->pawns & b->white;
        while (pawns) {
            int sq = ctz(pawns); pawns &= pawns - 1;
            int f = FILE(sq);
            int dir = 1;
            U64 sup = 0;
            if (f > 0) sup |= SQUARES[sq + dir*8 - 1];
            if (f < 7) sup |= SQUARES[sq + dir*8 + 1];
            if (!(b->pawns & (sup | SQUARES[sq + dir*8]))) mg -= P.p_backward;
        }

        pawns = b->pawns & b->black;
        seen = 0;
        while (pawns) {
            int sq = ctz(pawns); pawns &= pawns - 1;
            int f = FILE(sq);
            if (seen & FILES[f]) mg += P.p_doubled;
            seen |= FILES[f];
        }
        pawns = b->pawns & b->black;
        while (pawns) {
            int sq = ctz(pawns); pawns &= pawns - 1;
            int f = FILE(sq);
            U64 nb = (f > 0 ? FILES[f-1] : 0) | (f < 7 ? FILES[f+1] : 0);
            if (!(b->pawns & nb)) mg += P.p_isolated;
        }
        pawns = b->pawns & b->black;
        while (pawns) {
            int sq = ctz(pawns); pawns &= pawns - 1;
            int f = FILE(sq);
            int dir = -1;
            U64 sup = 0;
            if (f > 0) sup |= SQUARES[sq + dir*8 - 1];
            if (f < 7) sup |= SQUARES[sq + dir*8 + 1];
            if (!(b->pawns & (sup | SQUARES[sq + dir*8]))) mg += P.p_backward;
        }
    }

    // Bishop pair
    if (popcount(b->bishops & b->white) >= 2) mg += P.bishop_pair;
    if (popcount(b->bishops & b->black) >= 2) mg -= P.bishop_pair;

    // Mobility
    {
        U64 occ = b->white | b->black;
        U64 nb = b->knights & b->white;
        while (nb) { int s = ctz(nb); nb &= nb-1; int n = popcount(KNIGHT_ATTACKS[s] & ~occ); mg += P.mobN * (n-4); }
        nb = b->knights & b->black;
        while (nb) { int s = ctz(nb); nb &= nb-1; int n = popcount(KNIGHT_ATTACKS[s] & ~occ); mg -= P.mobN * (n-4); }
        nb = b->bishops & b->white;
        while (nb) { int s = ctz(nb); nb &= nb-1; int n = popcount(bb_bishop_attacks(s, occ) & ~occ); mg += P.mobB * (n-7); }
        nb = b->bishops & b->black;
        while (nb) { int s = ctz(nb); nb &= nb-1; int n = popcount(bb_bishop_attacks(s, occ) & ~occ); mg -= P.mobB * (n-7); }
        nb = b->rooks & b->white;
        while (nb) { int s = ctz(nb); nb &= nb-1; int n = popcount(bb_rook_attacks(s, occ) & ~occ); mg += P.mobR * (n-7); }
        nb = b->rooks & b->black;
        while (nb) { int s = ctz(nb); nb &= nb-1; int n = popcount(bb_rook_attacks(s, occ) & ~occ); mg -= P.mobR * (n-7); }
        nb = b->queens & b->white;
        while (nb) { int s = ctz(nb); nb &= nb-1; int n = popcount((bb_bishop_attacks(s, occ)|bb_rook_attacks(s, occ)) & ~occ); mg += P.mobQ * (n-14); }
        nb = b->queens & b->black;
        while (nb) { int s = ctz(nb); nb &= nb-1; int n = popcount((bb_bishop_attacks(s, occ)|bb_rook_attacks(s, occ)) & ~occ); mg -= P.mobQ * (n-14); }
    }

    // Passed pawns
    {
        U64 pawns = b->pawns & b->white;
        while (pawns) {
            int sq = ctz(pawns); pawns &= pawns - 1;
            int r = RANK(sq);
            mg += P.passed_mg[0][r];
            eg += P.passed_eg[0][r];
        }
        pawns = b->pawns & b->black;
        while (pawns) {
            int sq = ctz(pawns); pawns &= pawns - 1;
            int r = RANK(sq);
            mg -= P.passed_mg[1][7-r];
            eg -= P.passed_eg[1][7-r];
        }
    }

    // Rook on file
    {
        U64 rooks = b->rooks & b->white;
        while (rooks) {
            int sq = ctz(rooks); rooks &= rooks - 1;
            int f = FILE(sq);
            U64 fp = b->pawns & FILES[f];
            if (!(fp & b->white)) mg += (fp & b->black) ? P.rook_semiopen : P.rook_open;
        }
        rooks = b->rooks & b->black;
        while (rooks) {
            int sq = ctz(rooks); rooks &= rooks - 1;
            int f = FILE(sq);
            U64 fp = b->pawns & FILES[f];
            if (!(fp & b->black)) mg -= (fp & b->white) ? P.rook_semiopen : P.rook_open;
        }
    }

    mg += P.tempo;

    // Tapered
    return (mg * phase + eg * (24 - phase)) / 24.0;
}

static void recompute_scores(void) {
    for (int i = 0; i < g_count; i++) {
        Board b;
        board_load_fen(&b, g_fen[i]);
        double s = eval_position(&b);
        g_score[i] = (int)(s * g_scale);
    }
}

//=============================================================================
// Compute gradient contribution for one position
// ds/dp for each param is the number of attack squares / pawns affected
//=============================================================================
static void compute_grad_pos(int idx) {
    double s = (double)g_score[idx];
    double z = g_result[idx];
    double q = sigmoid(s);
    // dq/ds = q*(1-q)*ln(10)/400
    double dq = q * (1.0 - q) * 0.00575646273; // ln(10)/400
    double delta = 2.0 * (q - z) * dq;

    Board b;
    board_load_fen(&b, g_fen[idx]);
    U64 occ = b.white | b.black;

    // Mobility gradients
    U64 nb = b.knights & b.white;
    while (nb) { int sq = ctz(nb); nb &= nb-1; grad.mobN += delta * (popcount(KNIGHT_ATTACKS[sq] & ~occ) - 4); }
    nb = b.knights & b.black;
    while (nb) { int sq = ctz(nb); nb &= nb-1; grad.mobN -= delta * (popcount(KNIGHT_ATTACKS[sq] & ~occ) - 4); }
    nb = b.bishops & b.white;
    while (nb) { int sq = ctz(nb); nb &= nb-1; grad.mobB += delta * (popcount(bb_bishop_attacks(sq, occ) & ~occ) - 7); }
    nb = b.bishops & b.black;
    while (nb) { int sq = ctz(nb); nb &= nb-1; grad.mobB -= delta * (popcount(bb_bishop_attacks(sq, occ) & ~occ) - 7); }
    nb = b.rooks & b.white;
    while (nb) { int sq = ctz(nb); nb &= nb-1; grad.mobR += delta * (popcount(bb_rook_attacks(sq, occ) & ~occ) - 7); }
    nb = b.rooks & b.black;
    while (nb) { int sq = ctz(nb); nb &= nb-1; grad.mobR -= delta * (popcount(bb_rook_attacks(sq, occ) & ~occ) - 7); }
    nb = b.queens & b.white;
    while (nb) { int sq = ctz(nb); nb &= nb-1; grad.mobQ += delta * (popcount((bb_bishop_attacks(sq, occ)|bb_rook_attacks(sq, occ)) & ~occ) - 14); }
    nb = b.queens & b.black;
    while (nb) { int sq = ctz(nb); nb &= nb-1; grad.mobQ -= delta * (popcount((bb_bishop_attacks(sq, occ)|bb_rook_attacks(sq, occ)) & ~occ) - 14); }

    // Bishop pair
    if (popcount(b.bishops & b.white) >= 2) grad.bishop_pair += delta;
    if (popcount(b.bishops & b.black) >= 2) grad.bishop_pair -= delta;

    // Rook open/semiopen
    U64 rooks = b.rooks & b.white;
    while (rooks) {
        int sq = ctz(rooks); rooks &= rooks - 1;
        int f = FILE(sq);
        U64 fp = b.pawns & FILES[f];
        if (!(fp & b.white)) {
            if (fp & b.black) grad.rook_semiopen += delta;
            else             grad.rook_open += delta;
        }
    }
    rooks = b.rooks & b.black;
    while (rooks) {
        int sq = ctz(rooks); rooks &= rooks - 1;
        int f = FILE(sq);
        U64 fp = b.pawns & FILES[f];
        if (!(fp & b.black)) {
            if (fp & b.white) grad.rook_semiopen -= delta;
            else             grad.rook_open -= delta;
        }
    }

    // Tempo
    grad.tempo += delta;

    // Passed pawn gradients
    U64 pawns = b.pawns & b.white;
    while (pawns) {
        int sq = ctz(pawns); pawns &= pawns - 1;
        int r = RANK(sq);
        grad.passed_mg[0][r] += delta;
        grad.passed_eg[0][r] += delta;
    }
    pawns = b.pawns & b.black;
    while (pawns) {
        int sq = ctz(pawns); pawns &= pawns - 1;
        int r = RANK(sq);
        grad.passed_mg[1][7-r] -= delta;
        grad.passed_eg[1][7-r] -= delta;
    }

    // Pawn structure gradients
    pawns = b.pawns & b.white;
    U64 seen = 0;
    while (pawns) {
        int sq = ctz(pawns); pawns &= pawns - 1;
        int f = FILE(sq);
        if (seen & FILES[f]) grad.p_doubled -= delta;
        seen |= FILES[f];
    }
    pawns = b.pawns & b.white;
    while (pawns) {
        int sq = ctz(pawns); pawns &= pawns - 1;
        int f = FILE(sq);
        U64 nb = (f > 0 ? FILES[f-1] : 0) | (f < 7 ? FILES[f+1] : 0);
        if (!(b.pawns & nb)) grad.p_isolated -= delta;
    }
    pawns = b.pawns & b.black;
    seen = 0;
    while (pawns) {
        int sq = ctz(pawns); pawns &= pawns - 1;
        int f = FILE(sq);
        if (seen & FILES[f]) grad.p_doubled += delta;
        seen |= FILES[f];
    }
    pawns = b.pawns & b.black;
    while (pawns) {
        int sq = ctz(pawns); pawns &= pawns - 1;
        int f = FILE(sq);
        U64 nb = (f > 0 ? FILES[f-1] : 0) | (f < 7 ? FILES[f+1] : 0);
        if (!(b.pawns & nb)) grad.p_isolated += delta;
    }
}

//=============================================================================
// Total cross-entropy error
//=============================================================================
static double total_error(void) {
    double E = 0;
    for (int i = 0; i < g_count; i++) {
        double q = sigmoid((double)g_score[i]);
        if (q < 1e-10) q = 1e-10;
        if (q > 1-1e-10) q = 1-1e-10;
        E -= g_result[i] * log(q) + (1 - g_result[i]) * log(1 - q);
    }
    return E;
}

//=============================================================================
// Apply gradients (SGD + L2 regularization)
//=============================================================================
static void apply_gradients(double lr, double lambda) {
    #define UPDATE(param, scale) do { \
        if (active.param) { \
            double g = grad.param + lambda * P.param; \
            P.param -= lr * lr_scales.param * g; \
        } \
    } while (0)

    UPDATE(mobN, 1.0);
    UPDATE(mobB, 1.0);
    UPDATE(mobR, 1.0);
    UPDATE(mobQ, 1.0);
    UPDATE(bishop_pair, 1.0);
    UPDATE(rook_open, 1.0);
    UPDATE(rook_semiopen, 1.0);
    UPDATE(tempo, 1.0);
    UPDATE(p_doubled, 1.0);
    UPDATE(p_isolated, 1.0);
    UPDATE(p_backward, 1.0);

    for (int s = 0; s < 2; s++)
        for (int r = 0; r < 8; r++) {
            if (active.passed_mg[s][r]) {
                double g = grad.passed_mg[s][r] + lambda * P.passed_mg[s][r];
                P.passed_mg[s][r] -= lr * g;
                if (P.passed_mg[s][r] < 0) P.passed_mg[s][r] = 0;
            }
            if (active.passed_eg[s][r]) {
                double g = grad.passed_eg[s][r] + lambda * P.passed_eg[s][r];
                P.passed_eg[s][r] -= lr * g;
                if (P.passed_eg[s][r] < 0) P.passed_eg[s][r] = 0;
            }
        }

    // Clamp mobility to reasonable range
    if (P.mobN < 0) P.mobN = 0; if (P.mobN > 20) P.mobN = 20;
    if (P.mobB < 0) P.mobB = 0; if (P.mobB > 20) P.mobB = 20;
    if (P.mobR < 0) P.mobR = 0; if (P.mobR > 20) P.mobR = 20;
    if (P.mobQ < 0) P.mobQ = 0; if (P.mobQ > 20) P.mobQ = 20;
    if (P.bishop_pair < 0) P.bishop_pair = 0;
    if (P.tempo < -30) P.tempo = -30; if (P.tempo > 30) P.tempo = 30;
}

static void zero_grad(void) {
    memset(&grad, 0, sizeof(grad));
}

//=============================================================================
// Initialize params from current eval constants
//=============================================================================
static void init_params(void) {
    P.mobN = MOBILITY_KNIGHT;
    P.mobB = MOBILITY_BISHOP;
    P.mobR = MOBILITY_ROOK;
    P.mobQ = MOBILITY_QUEEN;
    P.p_doubled   = 10.0;
    P.p_isolated  = 10.0;
    P.p_backward  = 8.0;
    P.bishop_pair = 30.0;
    P.rook_open   = ROOK_OPEN_FILE;
    P.rook_semiopen = ROOK_SEMIOPEN;
    P.tempo = 10.0;

    for (int s = 0; s < 2; s++)
        for (int r = 0; r < 8; r++) {
            P.passed_mg[s][r] = PASSED_PAWN_MG[s][r];
            P.passed_eg[s][r] = PASSED_PAWN_EG[s][r];
        }

    // All active
    memset(&active, 1, sizeof(active));
    memset(&lr_scales, 1, sizeof(lr_scales));

    // Per-param learning rate scales (rough heuristic: larger params need smaller lr)
    lr_scales.mobN = 0.01;
    lr_scales.mobB = 0.01;
    lr_scales.mobR = 0.01;
    lr_scales.mobQ = 0.01;
    lr_scales.bishop_pair = 0.05;
    lr_scales.rook_open = 0.05;
    lr_scales.rook_semiopen = 0.05;
    lr_scales.tempo = 0.05;
}

//=============================================================================
// Load EPD/PGN training file
// Format: FEN "result" on each line
//=============================================================================
static int load_file(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) { fprintf(stderr, "tuner: cannot open %s\n", path); return -1; }

    char line[512];
    int n = 0;
    while (fgets(line, sizeof(line), f) && n < MAX_POS) {
        char *q1 = strchr(line, '"');
        if (!q1) continue;
        char *q2 = strchr(q1 + 1, '"');
        if (!q2) continue;
        *q1 = '\0';

        float res;
        if (strncmp(q1+1, "1-0", 3) == 0)      res = 1.0;
        else if (strncmp(q1+1, "0-1", 3) == 0) res = 0.0;
        else if (strncmp(q1+1, "1/2", 3) == 0) res = 0.5;
        else continue;

        if (strlen(line) < 20) continue;
        strncpy(g_fen[n], line, 255);
        g_fen[n][255] = '\0';
        g_result[n] = res;
        n++;
    }
    fclose(f);
    return n;
}

//=============================================================================
// Print tuned eval constants (ready to paste into eval.h)
//=============================================================================
static void print_c_output(void) {
    printf("\n// ============================================================\n");
    printf("// TUNED EVAL PARAMETERS — paste these into include/eval.h\n");
    printf("// ============================================================\n\n");
    printf("#define MOBILITY_KNIGHT  %.0f\n", P.mobN);
    printf("#define MOBILITY_BISHOP  %.0f\n", P.mobB);
    printf("#define MOBILITY_ROOK    %.0f\n", P.mobR);
    printf("#define MOBILITY_QUEEN   %.0f\n", P.mobQ);
    printf("#define ROOK_OPEN_FILE   %.0f\n", P.rook_open);
    printf("#define ROOK_SEMIOPEN    %.0f\n", P.rook_semiopen);
    printf("#define OUTPOST_MIN      8\n");
    printf("#define OUTPOST_MAX     24\n");
    printf("#define BISHOP_PAIR_SCORE %.0f\n", P.bishop_pair);
    printf("\nstatic const int PASSED_PAWN_MG[2][8] = {\n");
    for (int s = 0; s < 2; s++) {
        printf("  { ");
        for (int r = 0; r < 8; r++) printf("%3.0f%s", P.passed_mg[s][r], r < 7 ? ", " : "");
        printf(" }%s\n", s == 0 ? "," : "");
    }
    printf("};\n\nstatic const int PASSED_PAWN_EG[2][8] = {\n");
    for (int s = 0; s < 2; s++) {
        printf("  { ");
        for (int r = 0; r < 8; r++) printf("%3.0f%s", P.passed_eg[s][r], r < 7 ? ", " : "");
        printf(" }%s\n", s == 0 ? "," : "");
    }
    printf("};\n\n");
    printf("// Pawn penalties\n");
    printf("#define DOUBLED_PAWN_PEN    %.0f\n", P.p_doubled);
    printf("#define ISOLATED_PAWN_PEN   %.0f\n", P.p_isolated);
    printf("#define BACKWARD_PAWN_PEN   %.0f\n", P.p_backward);
    printf("\n// ============================================================\n\n");
}

//=============================================================================
// Print progress
//=============================================================================
static void print_status(int epoch, double error, double lr) {
    printf("epoch %3d: error=%.6f  lr=%.6f  |  ", epoch, error, lr);
    printf("mobN=%.1f mobB=%.1f mobR=%.1f mobQ=%.1f  |  ", P.mobN, P.mobB, P.mobR, P.mobQ);
    printf("bp=%.0f ro=%.0f rs=%.0f  |  tempo=%.0f\n",
        P.bishop_pair, P.rook_open, P.rook_semiopen, P.tempo);
}

//=============================================================================
// Main tuning loop
//=============================================================================
void tuner_run(const char *data_file, int epochs, double base_lr, double lambda, int batch) {
    printf("Texel tuner initializing...\n");

    init_params();

    int n = load_file(data_file);
    if (n <= 0) {
        fprintf(stderr, "tuner: no positions loaded from %s\n", data_file);
        return;
    }
    g_count = n;
    printf("Loaded %d positions from %s\n", n, data_file);

    recompute_scores();
    double error = total_error();
    printf("Initial error: %.6f\n", error);

    double lr = base_lr;

    for (int epoch = 1; epoch <= epochs; epoch++) {
        zero_grad();

        // Accumulate gradients
        for (int i = 0; i < g_count; i++)
            compute_grad_pos(i);

        apply_gradients(lr, lambda);

        // Re-evaluate every 5 epochs to get true error
        if (epoch % 5 == 0) {
            recompute_scores();
            error = total_error();
            print_status(epoch, error, lr);
        } else {
            print_status(epoch, error, lr);
        }

        // Learning rate decay
        if (epoch % 20 == 0) lr *= 0.9;
    }

    recompute_scores();
    error = total_error();
    printf("\nFinal error: %.6f\n", error);
    print_c_output();
}
