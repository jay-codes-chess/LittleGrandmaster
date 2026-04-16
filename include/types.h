#ifndef TYPES_H
#define TYPES_H

#include <stdint.h>
#include <stdbool.h>

//=============================================================================
// Types
//=============================================================================

typedef uint64_t U64;
typedef uint32_t U32;
typedef uint16_t U16;
typedef uint8_t  U8;

#define MAX_MOVES     256
#define MAX_PV        64
#define MAX_DEPTH     64
#define MAX_THREADS   64
#define HASH_SIZE     256  // MB

//=============================================================================
// Constants
//=============================================================================

#define EMPTY    0
#define W_PAWN   1
#define W_KNIGHT 2
#define W_BISHOP 3
#define W_ROOK   4
#define W_QUEEN  5
#define W_KING   6
#define B_PAWN   7
#define B_KNIGHT 8
#define B_BISHOP 9
#define B_ROOK   10
#define B_QUEEN  11
#define B_KING   12

#define WHITE    true
#define BLACK    false

// Castling
#define WK_CASTLE  1
#define WQ_CASTLE  2
#define BK_CASTLE  4
#define BQ_CASTLE  8
#define ALL_CASTLES (WK_CASTLE | WQ_CASTLE | BK_CASTLE | BQ_CASTLE)

// Move flags
#define FLAG_CAPTURE     1
#define FLAG_PROMOTION   2
#define FLAG_CASTLE      4
#define FLAG_PAWN2       8   // pawn double push (for ep)
#define FLAG_EP_CAPTURE  16

// Squares
#define A1  56
#define B1  57
#define C1  58
#define D1  59
#define E1  60
#define F1  61
#define G1  62
#define H1  63
#define A2  48
#define B2  49
#define C2  50
#define D2  51
#define E2  52
#define F2  53
#define G2  54
#define H2  55
#define A3  40
#define B3  41
#define C3  42
#define D3  43
#define E3  44
#define F3  45
#define G3  46
#define H3  47
#define A4  32
#define B4  33
#define C4  34
#define D4  35
#define E4  36
#define F4  37
#define G4  38
#define H4  39
#define A5  24
#define B5  25
#define C5  26
#define D5  27
#define E5  28
#define F5  29
#define G5  30
#define H5  31
#define A6  16
#define B6  17
#define C6  18
#define D6  19
#define E6  20
#define F6  21
#define G6  22
#define H6  23
#define A7   8
#define B7   9
#define C7  10
#define D7  11
#define E7  12
#define F7  13
#define G7  14
#define H7  15
#define A8   0
#define B8   1
#define C8   2
#define D8   3
#define E8   4
#define F8   5
#define G8   6
#define H8   7

// Helpers
#define FILE(sq)   ((sq) & 7)
#define RANK(sq)   ((sq) >> 3)
#define SQUARE(f,r) ((r) * 8 + (f))
#define SQ(f,r)    ((r) * 8 + (f))
#define between(a,b,c) ((unsigned)((b)-(a)) <= (unsigned)((c)-(a)))
#define ctz(x)     __builtin_ctzll(x)
#define clz(x)      __builtin_clzll(x)
#define popcount(x) __builtin_popcountll(x)

#define max(a,b) ((a) > (b) ? (a) : (b))
#define min(a,b) ((a) < (b) ? (a) : (b))

//=============================================================================
// Move
//=============================================================================

typedef struct {
    int from, to;
    int piece;
    int captured;
    int promo;
    int flags;
    int score;
} Move;

// Board state for save/restore
typedef struct {
    U64 hash;
    int castle;
    int ep_square;
    int halfmove;
    int checkers;
} BoardState;

//=============================================================================
// Board
//=============================================================================

typedef struct {
    U64 pawns, knights, bishops, rooks, queens, kings;
    U64 white, black;
    bool side;
    int ep_square;
    int castle;
    int halfmove;
    int fullmove;
    U64 hash;

    // Repetition
    U64 rep_keys[256];
    int rep_count;

    // State stack
    BoardState state[256];
    int state_idx;
} Board;

//=============================================================================
// Move List
//=============================================================================

typedef struct {
    Move moves[MAX_MOVES];
    int count;
} Movelist;

//=============================================================================
// Transposition Table
//=============================================================================

typedef struct {
    U64 key;
    Move move;
    int depth;
    int score;
    uint8_t flag;    // 0=alpha, 1=exact, 2=beta
    uint8_t age;
} TTEntry;

typedef struct {
    TTEntry *entries;
    int size;
    uint64_t mask;
    uint8_t age;
    uint64_t hits, misses, writes;
} TranspositionTable;

//=============================================================================
// Search
//=============================================================================

typedef struct {
    int depth;
    int seldepth;
    int64_t time_start;
    int time_limit;
    int nodes;
    int64_t tb_hits;
    bool stop;
    int null_pruning;
    int fp_margin;
    int razoring_margin;
    int lmp_cutoff;
    int history_cutoff;
} SearchInfo;

typedef struct {
    Move pv[MAX_DEPTH];
    int pv_length[MAX_DEPTH];
} PV;

typedef struct {
    Move best_move;
    int best_score;
    int nodes;
    int depth;
    bool found;
} SearchResult;

// History heuristic
typedef struct {
    int16_t history[2][64][64];
    int16_t counter_moves[2][64][64];
    int16_t followup_moves[2][64][64];
} History;

// Killer moves
typedef struct {
    Move killers[MAX_DEPTH][2];
} KillerTable;

//=============================================================================
// Evaluation
//=============================================================================

// PeSTO material values (mg/eg)
#define V_MG(p)  g_vmg[(p)]
#define V_EG(p)  g_veg[(p)]

// Phase values
#define PHASE_NN    1
#define PHASE_BISHOP 1
#define PHASE_ROOK   2
#define PHASE_QUEEN  4
#define TOTAL_PHASE  24

// Evaluation trace
typedef struct {
    int mg_score, eg_score;
    int phase;
    int material;
    int imbalance;
    int pawns;
    int pcsq;
    int mobility;
    int king_safety;
    int passed_pawns;
    int space;
} EvalTrace;

//=============================================================================
// UCI
//=============================================================================

typedef struct {
    int hash;
    int threads;
    int clear_hash;
    bool uci;
    bool debug;
    int multi_pv;
    int64_t uci_hash;
    int uci_elo;
} UCIOption;

#endif // TYPES_H
