#include <stdio.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include "search.h"
#include "board.h"
#include "moves.h"
#include "eval.h"
#include "hash.h"
#include "bitboard.h"

//=============================================================================
// Global Search State
//=============================================================================

volatile bool stop_flag = false;
static PV s_pv[MAX_DEPTH];
static History s_history;
static KillerTable s_killers;
static Move s_counter_moves[64][64];
static Move s_best_move_this_iter;
static int s_best_score_this_iter;

//=============================================================================
// Search Info
//=============================================================================

void search_stop(void) {
    stop_flag = true;
}

static void init_search_info(SearchInfo *info, int depth) {
    info->depth = depth;
    info->seldepth = 0;
    info->nodes = 0;
    info->tb_hits = 0;
    info->stop = false;
    info->time_start = 0;
    info->time_limit = 0;
    info->null_pruning = 3;
    info->fp_margin = 100;
    info->razoring_margin = 450;
    info->lmp_cutoff = 3;
    info->history_cutoff = 2000;
}

//=============================================================================
// Time Management
//=============================================================================

void time_init(SearchInfo *info, int time, int inc, int movestogo, int depth) {
    (void)inc;
    (void)movestogo;
    info->time_start = (int64_t)clock() * 1000 / CLOCKS_PER_SEC;
    if (depth > 0) {
        info->time_limit = time / depth;
    } else {
        info->time_limit = time / 20;
    }
}

void time_check(SearchInfo *info, const Board *b) {
    (void)b;
    if (!info->time_limit) return;
    int64_t elapsed = (int64_t)clock() * 1000 / CLOCKS_PER_SEC - info->time_start;
    if (elapsed >= info->time_limit * 95 / 100) {
        stop_flag = true;
    }
}

bool should_stop(const SearchInfo *info, const Board *b) {
    (void)b;
    return stop_flag || (info->stop);
}

//=============================================================================
// Mate Score Utilities
//=============================================================================

bool is_mate_score(int score) {
    return abs(score) > 29000;
}

int mate_from_score(int score) {
    return score > 0 ? 30000 - score : -30000 - score;
}

int mate_to_score(int mate_in) {
    return mate_in > 0 ? 30000 - mate_in : -30000 - mate_in;
}

int value_to_tt(int v, int ply) {
    if (v > 28000) return v + ply;
    if (v < -28000) return v - ply;
    return v;
}

int value_from_tt(int v, int ply) {
    if (v > 28000) return v - ply;
    if (v < -28000) return v + ply;
    return v;
}

bool within_window(int alpha, int score, int beta) {
    return score > alpha && score < beta;
}

//=============================================================================
// Reductions / Pruning
//=============================================================================

int lmr(int depth, int move_num, bool is_capture, bool is_promo, bool in_check, bool gives_check) {
    if (depth < 2 || move_num < 3) return 0;
    if (in_check || gives_check) return 0;

    int reduction = 1;
    reduction += log(depth) * log(move_num) / 2;
    if (is_capture) reduction--;
    if (is_promo) reduction--;

    return max(0, min(reduction, depth - 1));
}

bool futility_prune(int depth, int alpha, int eval) {
    if (depth <= 0) return false;
    return eval >= alpha + 150 * depth;
}

bool razoring(int depth, int alpha) {
    return depth <= 3 && alpha < -800;
}

bool reverse_futility(int depth, int beta, int eval) {
    if (depth <= 5) return false;
    return eval - 200 * depth >= beta;
}

int search_null(Board *b, int depth, int R, int alpha, int beta, SearchInfo *info, int ply) {
    (void)b; (void)depth; (void)R; (void)alpha; (void)beta; (void)info; (void)ply;
    return 0;
}

//=============================================================================
// History Heuristic
//=============================================================================

void history_init(History *h) {
    memset(h, 0, sizeof(History));
}

void history_clear(History *h) {
    memset(h, 0, sizeof(History));
}

int history_score(const History *h, bool side, Move *m) {
    return h->history[side][m->from][m->to];
}

void history_add_successful(History *h, bool side, Move *m, int depth) {
    int bonus = min(3000, 1000 + 1000 * depth);
    int *entry = &h->history[side][m->from][m->to];
    *entry += bonus - (*entry * bonus / 3000);
    if (*entry > 3000) *entry = 3000;
}

void history_add_failure(History *h, bool side, Move *m) {
    int *entry = &h->history[side][m->from][m->to];
    *entry -= 200;
    if (*entry < -3000) *entry = -3000;
}
void history_update(History *h, Move *m, int depth, int side) {
    history_add_successful(h, side, m, depth);
}

// Counter moves
void counter_move(Board *b, Move *m, Move prev[MAX_DEPTH]) {
    (void)b; (void)m; (void)prev;
}

//=============================================================================
// Killer Moves
//=============================================================================

void killers_clear(KillerTable *kt) {
    memset(kt, 0, sizeof(KillerTable));
}

void killers_add(KillerTable *kt, Move *m, int ply) {
    if (ply < MAX_DEPTH) {
        if (!move_equal(&kt->killers[ply][0], m)) {
            kt->killers[ply][1] = kt->killers[ply][0];
            kt->killers[ply][0] = *m;
        }
    }
}

bool is_killer(const KillerTable *kt, Move *m, int ply) {
    if (ply < MAX_DEPTH)
        return move_equal(&kt->killers[ply][0], m) || move_equal(&kt->killers[ply][1], m);
    return false;
}

//=============================================================================
// Quiescence Search
//=============================================================================

// Minimal qsearch - captures only with hard ply limit
int search_quiescence(Board *b, int alpha, int beta, SearchInfo *info, int ply) {
    // Hard ply limit - prevent infinite recursion
    if (ply >= 6) return evaluate(b);
    
    info->nodes++;
    
    // Standing pat score
    int stand_pat = evaluate(b);
    if (stand_pat >= beta) return beta;
    if (stand_pat > alpha) alpha = stand_pat;
    
    // Generate captures
    Movelist captures;
    generate_captures(b, &captures);
    score_moves_qsearch(&captures, b);
    
    // Limit captures to prevent explosion
    // At qsearch ply 0-1: search more captures
    // At qsearch ply 2+: fewer captures
    int max_captures = (ply < 2) ? 8 : 4;
    if (captures.count > max_captures) captures.count = max_captures;
    
    int best_score = alpha;
    
    for (int i = 0; i < captures.count; i++) {
        pick_best(&captures, i);
        
        Board board_copy = *b;
        board_do_move(b, &captures.moves[i]);
        
        // Skip moves that leave us in check
        if (board_in_check(b)) {
            *b = board_copy;
            continue;
        }
        
        int score = -search_quiescence(b, -beta, -alpha, info, ply + 1);
        *b = board_copy;
        
        if (score > best_score) best_score = score;
        if (best_score >= beta) return best_score;
        if (best_score > alpha) alpha = best_score;
    }
    
    return best_score;
}

//=============================================================================
// Main Alpha-Beta Search (PV)
//=============================================================================

int search_pv(Board *b, int depth, int alpha, int beta, SearchInfo *info, PV *pv, int ply) {
    if (stop_flag) return 0;

    // Hard ply limit to prevent stack overflow - must be before any local allocations
    if (ply >= 6) return evaluate(b);

    info->nodes++;
    if (ply > info->seldepth) info->seldepth = ply;

    // Draw detection
    if (ply && is_repetition(b)) return ply - 30;
    if (ply && is_fifty_moves(b)) return 0;

    if (depth <= 0) {
            return search_quiescence(b, alpha, beta, info, ply);
    }

    bool in_check = board_in_check(b);
    if (in_check) depth++;  // Check extension

    // Mate distance pruning (only at ply >= 6 to prevent immediate -30000 at root)
    if (ply >= 6) {
        int mate_value = mate_from_score(ply);
        if (mate_value < beta) {
            beta = mate_value;
            if (alpha >= mate_value) return mate_value;
        }
        if (mate_value > alpha) {
            alpha = mate_value;
            if (beta <= mate_value) return mate_value;
        }
    }

    // Transposition table probe
    TTEntry tt_e;
    Move tt_move = {0};
    bool tt_hit = tt_probe(b->hash, &tt_e);
    if (tt_hit && tt_e.depth >= depth) {
        tt_move = tt_e.move;
        int tt_score = value_from_tt(tt_e.score, ply);
        if (tt_e.flag == TT_FLAG_EXACT) return tt_score;
        if (tt_e.flag == TT_FLAG_ALPHA && tt_score <= alpha) return alpha;
        if (tt_e.flag == TT_FLAG_BETA && tt_score >= beta) return beta;
    }

    // Razor Search
    if (!in_check && depth <= 3 && !tt_hit) {
        int eval = evaluate(b);
        if (depth == 1 && eval + 280 < alpha) {
            return search_quiescence(b, alpha, beta, info, ply);
        }
        if (depth == 2 && eval + 350 < alpha) {
            int s = search_quiescence(b, alpha, beta, info, ply);
            if (s < alpha) return s;
        }
    }

    // Null move (not in endgames)
    if (!in_check && depth >= 3 && ply > 0 && info->null_pruning > 0) {
        int R = depth > 6 ? 4 : 3;
        Board copy = *b;
        b->side = !b->side;
        if (b->ep_square >= 0) {
            b->hash ^= hash_ep[b->ep_square & 7];
            b->ep_square = -1;
        }
        int null_score = -search_pv(b, depth - 1 - R, -beta, -beta + 1, info, pv, ply + 1);
        *b = copy;
        if (null_score >= beta) return beta;
    }

    // Generate moves
    Movelist ml;
    generate_moves(b, &ml);
    score_moves(&ml, b, &tt_move, depth, &s_history, &s_killers, ply);

    // Move ordering: TT move already at top via scoring
    sort_moves_insertion(&ml, 0);

    int legal = 0;
    int best_score = -30000;
    Move best_move = {0};
    int alpha_orig = alpha;
    int score_type = TT_FLAG_ALPHA;

    for (int i = 0; i < ml.count; i++) {
        pick_best(&ml, i);
        Move m = ml.moves[i];

        Board copy = *b;
        board_do_move(b, &m);

        if (board_in_check(b)) {
            *b = copy;
            continue;
        }


        legal++;

        if (should_stop(info, b)) {
            *b = copy;
            return 0;
        }

        time_check(info, b);
        if (should_stop(info, b)) {
            *b = copy;
            return 0;
        }

        bool is_capture = m.flags & FLAG_CAPTURE;
        bool is_promo = m.flags & FLAG_PROMOTION;
        bool gives_check = board_in_check(b);

        // Late Move Reductions
        int reduction = 0;
        if (legal > 4 && depth >= 3 && !is_capture && !is_promo) {
            reduction = lmr(depth, legal, is_capture, is_promo, in_check, gives_check);

            // Futility pruning
            if (!in_check && reduction > 0) {
                int eval = evaluate(b);
                if (futility_prune(depth - reduction, alpha, eval)) {
                    reduction = 0;
                }
            }

            // Reverse futility
            if (depth >= 8 && !is_capture && !gives_check) {
                int eval = evaluate(b);
                if (reverse_futility(depth, beta, eval)) {
                    reduction = max(reduction, 2);
                }
            }
        }

        int new_depth = depth - 1 - reduction;

        int score;
        if (legal == 1) {
            // PVS
            score = -search_pv(b, new_depth, -beta, -alpha, info, pv, ply + 1);
        } else {
            // Null window search
            PV child_pv2;
            memset(&child_pv2, 0, sizeof(child_pv2));
            score = -search_pv(b, new_depth, -alpha - 1, -alpha, info, &child_pv2, ply + 1);
            if (score > alpha && score < beta) {
                // Research with full window
                PV child_pv3;
                memset(&child_pv3, 0, sizeof(child_pv3));
                score = -search_pv(b, new_depth, -beta, -alpha, info, &child_pv3, ply + 1);
                if (score > best_score) {
                    best_score = score;
                    best_move = m;
                    pv->pv[ply] = m;
                    pv->pv_length[ply] = child_pv3.pv_length[ply + 1] + 1;
                    for (int j = ply + 1; j < child_pv3.pv_length[ply + 1] + 1; j++) {
                        pv->pv[j] = child_pv3.pv[j];
                    }
                }
            } else if (score > best_score) {
                best_score = score;
                best_move = m;
                pv->pv[ply] = m;
                pv->pv_length[ply] = child_pv2.pv_length[ply + 1] + 1;
                for (int j = ply + 1; j < child_pv2.pv_length[ply + 1] + 1; j++) {
                    pv->pv[j] = child_pv2.pv[j];
                }
            }
        }

        *b = copy;

        if (score > best_score) {
            best_score = score;
            best_move = m;
            // Update PV
            pv->pv[ply] = m;
            pv->pv_length[ply] = ply + 1;
            // Debug
            if (ply == 0) {
                char buf[8]; char buf2[8];
                move_to_uci(&m, buf, false);
            }
        }

        if (score >= beta) {
            if (!(m.flags & FLAG_CAPTURE)) {
                killers_add(&s_killers, &m, ply);
                history_add_successful(&s_history, !b->side, &m, depth);
            }
            tt_store(b->hash, &m, depth, value_to_tt(beta, ply), TT_FLAG_BETA);
            return beta;
        }

        if (score > alpha) {
            alpha = score;
            score_type = TT_FLAG_EXACT;
        }
    }

    if (legal == 0) {
        if (in_check) {
            return mate_from_score(ply);
        }
        return 0;  // Stalemate
    }

    if (best_move.from || best_move.to) {
        tt_store(b->hash, &best_move, depth, value_to_tt(best_score, ply), score_type);
    }

    return alpha;
}

//=============================================================================
// Iterative Deepening
//=============================================================================

void search_id_loop(Board *b, SearchInfo *info, int max_depth) {
    stop_flag = false;
    history_init(&s_history);
    killers_clear(&s_killers);
    memset(s_counter_moves, 0, sizeof(s_counter_moves));

    int completed_depth = 0;

    for (int depth = 1; depth <= max_depth && !stop_flag; depth++) {
        init_search_info(info, depth);

        int alpha = -30000;
        int beta = 30000;

        // Aspiration windows
        if (depth >= 4 && completed_depth > 0) {
            int prev_score = s_best_score_this_iter;
            int window = 25 + abs(prev_score) / 40;
            alpha = max(prev_score - window, -30000);
            beta = min(prev_score + window, 30000);
        }

        PV pv;
        memset(&pv, 0, sizeof(pv));

        int score = search_pv(b, depth, alpha, beta, info, &pv, 0);

        if (stop_flag) break;

        completed_depth = depth;
        s_best_score_this_iter = score;
        s_best_move_this_iter = pv.pv[0];

        // Log info
        int64_t elapsed = (int64_t)clock() * 1000 / CLOCKS_PER_SEC - info->time_start;
        int nps = elapsed > 0 ? (int)(info->nodes / (elapsed / 1000.0)) : 0;

        printf("info depth %d seldepth %d nodes %lld time %lld nps %d score cp %d pv",
            depth, info->seldepth, (long long)info->nodes, (long long)elapsed, nps, score);
        for (int i = 0; i < 30 && pv.pv[i].from; i++) {
            char buf[8];
            move_to_uci(&pv.pv[i], buf, false);
            printf(" %s", buf);
        }
        printf("\n");
        fflush(stdout);
    }

    // Store best move
    info->stop = true;
}

SearchResult search_start(Board *b, SearchInfo *info, int depth) {
    SearchResult result = {0};

    tt_age();
    tt_prefetch(b->hash);

    int max_depth = (depth > 0) ? depth : MAX_DEPTH;
    search_id_loop(b, info, max_depth);

    result.best_move = s_best_move_this_iter;
    result.best_score = s_best_score_this_iter;
    result.nodes = info->nodes;
    result.depth = info->depth;
    result.found = true;

    return result;
}
