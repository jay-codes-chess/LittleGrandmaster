#ifndef SEARCH_H
#define SEARCH_H

#include "types.h"
#include "board.h"

//=============================================================================
// Main Search
//=============================================================================

SearchResult search_start(Board *b, SearchInfo *info, int depth);
void search_stop(void);

// Alpha-beta with PV
int  search_pv(Board *b, int depth, int alpha, int beta, SearchInfo *info, PV *pv, int ply);
int  search_quiescence(Board *b, int alpha, int beta, SearchInfo *info, int ply);

// Internal iterative deepening
int  search_iid(Board *b, int depth, int alpha, int beta, SearchInfo *info, PV *pv, int ply);

//=============================================================================
// Reductions / Pruning
//=============================================================================

// Late Move Reductions
int  lmr(int depth, int move_num, bool is_capture, bool is_promotion, bool in_check, bool gives_check);

// Futility pruning
bool futility_prune(int depth, int alpha, int eval);

// Null move pruning
int  search_null(Board *b, int depth, int R, int alpha, int beta, SearchInfo *info, int ply);

// Razoring
bool razoring(int depth, int alpha);

// Reverse futility (static null move pruning)
bool reverse_futility(int depth, int beta, int eval);

// Singularity
int  singularity_search(Board *b, Move *m, int depth, int alpha, int beta, SearchInfo *info, int ply);

// Internal iterative deepening
void search_id_loop(Board *b, SearchInfo *info, int max_depth);

//=============================================================================
// Movepick
//=============================================================================

typedef struct {
    Movelist *ml;
    int stage;
    int cur;
    Move tt_move;
    Move killers[2];
    int depth;
    bool skip_quiets;
} Movepick;

// Stage constants
#define ST_GENERATE_ALL     0
#define ST_GENERATE_CAPT    1
#define ST_GENERATE_QUIET   2
#define ST_TT_MOVE          3
#define ST_GOOD_CAPT        4
#define ST_BAD_CAPT         5
#define ST_KILLER_1         6
#define ST_KILLER_2         7
#define ST_COUNTER          8
#define ST_FOLLOWUP         9
#define ST_QUIET            10
#define ST_BAD_QUIET        11
#define ST_DONE             12

void movepick_init(Movepick *mp, Movelist *ml, Move tt_move, Move killers[2], int depth, bool skip_quiets);
Move movepick_next(Movepick *mp, const Board *b, History *h);

//=============================================================================
// History Heuristic
//=============================================================================

void history_init(History *h);
void history_update(History *h, Move *m, int depth, int side);
void history_clear(History *h);

int  history_score(const History *h, bool side, Move *m);
void history_add_successful(History *h, bool side, Move *m, int depth);
void history_add_failure(History *h, bool side, Move *m);

// Counter moves
void counter_move(Board *b, Move *m, Move prev[MAX_DEPTH]);

//=============================================================================
// Killer Moves
//=============================================================================

void killers_clear(KillerTable *kt);
void killers_add(KillerTable *kt, Move *m, int ply);
bool is_killer(const KillerTable *kt, Move *m, int ply);

//=============================================================================
// Time Management
//=============================================================================

void time_init(SearchInfo *info, int time, int inc, int movestogo, int depth);
void time_check(SearchInfo *info, const Board *b);
bool should_stop(const SearchInfo *info, const Board *b);

//=============================================================================
// Search Utilities
//=============================================================================

bool is_mate_score(int score);
int  mate_from_score(int score);
int  mate_to_score(int mate_in);

int value_to_tt(int v, int ply);
int value_from_tt(int v, int ply);

// Check if a score is within aspiration window
bool within_window(int alpha, int score, int beta);

#endif // SEARCH_H
