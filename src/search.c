#include <stdio.h>
#include <string.h>
#include <stdlib.h>
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
static Move s_prev_move[MAX_DEPTH];
static Move s_best_move_this_iter;
static int s_best_score_this_iter;

// LMR reduction table: [is_pv][depth][move_number]
static double s_lmr_table[2][64][64];

// Piece values for qsearch delta pruning (PeSTO)
static const int s_piece_value[13] = {
    0, 100, 320, 330, 500, 900, 20000,
    100, 320, 330, 500, 900, 20000, 0
};

//=============================================================================
// Init
//=============================================================================

void search_init_lmr(void) {
    for (int is_pv = 0; is_pv < 2; is_pv++) {
        for (int d = 1; d < 64; d++) {
            for (int m = 1; m < 64; m++) {
                double base = is_pv ? 0.0 : 0.33;
                double v = base + log((double)d) * log((double)m) / (is_pv ? 3.50 : 2.25);
                if (v < 1.0) v = 0;
                else v += 0.5;
                if (v > d - 1) v = d - 1;
                s_lmr_table[is_pv][d][m] = v;
            }
        }
    }
}

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
    (void)inc; (void)movestogo;
    info->time_start = (int64_t)clock() * 1000 / CLOCKS_PER_SEC;
    if (depth > 0) info->time_limit = time / depth;
    else info->time_limit = time / 20;
}

void time_check(SearchInfo *info, const Board *b) {
    (void)b;
    if (!info->time_limit) return;
    int64_t elapsed = (int64_t)clock() * 1000 / CLOCKS_PER_SEC - info->time_start;
    if (elapsed >= info->time_limit * 95 / 100) stop_flag = true;
}

bool should_stop(const SearchInfo *info, const Board *b) {
    (void)b;
    return stop_flag || info->stop;
}

//=============================================================================
// Mate Score Utilities
//=============================================================================

bool is_mate_score(int score) { return abs(score) > 29000; }

int mate_from_score(int score) {
    return score > 0 ? 30000 - score : -30000 - score;
}

int mate_to_score(int mate_in) {
    return mate_in > 0 ? 30000 - mate_in : -30000 + mate_in;
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

//=============================================================================
// LMR
//=============================================================================

int calc_lmr(int depth, int move_num, bool is_capture, bool is_promo,
             bool in_check, bool gives_check, bool is_pv) {
    if (depth < 2 || move_num < 3) return 0;
    if (in_check || gives_check) return 0;
    double reduction = s_lmr_table[is_pv ? 1 : 0][depth][move_num];
    if (is_capture) reduction -= 0.5;
    if (is_promo) reduction -= 0.5;
    return (int)max(0, min(reduction, depth - 1));
}

// Futility at frontier nodes
bool futility_prune(int depth, int alpha, int eval) {
    if (depth <= 0) return false;
    return eval >= alpha + 100 + 60 * depth;
}

//=============================================================================
// History Heuristic
//=============================================================================

void history_init(History *h) { memset(h, 0, sizeof(History)); }
void history_clear(History *h) { memset(h, 0, sizeof(History)); }

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

void counter_move_record(Move *m, int ply) {
    if (ply < MAX_DEPTH && m->from != m->to) {
        s_counter_moves[m->from][m->to] = *m;
    }
}

//=============================================================================
// Killer Moves
//=============================================================================

void killers_clear(KillerTable *kt) { memset(kt, 0, sizeof(KillerTable)); }

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
// SEE (Static Exchange Evaluation) - simplified for speed
//=============================================================================

static int see_capture(const Board *b, Move *m) {
    int from = m->from;
    int to = m->to;
    int piece = board_piece_at(b, from);
    U64 occ = b->white | b->black;
    bool side = (b->white & SQUARES[from]) != 0;

    // Approximate value of captured piece
    int gain[32];
    int n = 0;
    gain[n++] = s_piece_value[board_piece_at(b, to)];

    // Remove attacking piece
    occ ^= SQUARES[from];
    bool attackers_side = !side;
    U64 bb_attacked = 0;

    // Get attacks on destination
    if (piece == W_PAWN || piece == B_PAWN) {
        if (FILE(from) > 0) bb_attacked |= SQUARES[side ? to - 9 : to + 7];
        if (FILE(from) < 7) bb_attacked |= SQUARES[side ? to - 7 : to + 9];
    } else if (piece == W_KNIGHT || piece == B_KNIGHT) {
        bb_attacked = KNIGHT_ATTACKS[to];
    } else if (piece == W_BISHOP || piece == B_BISHOP) {
        bb_attacked = bb_bishop_attacks(to, occ);
    } else if (piece == W_ROOK || piece == B_ROOK) {
        bb_attacked = bb_rook_attacks(to, occ);
    } else if (piece == W_QUEEN || piece == B_QUEEN) {
        bb_attacked = bb_bishop_attacks(to, occ) | bb_rook_attacks(to, occ);
    } else if (piece == W_KING || piece == B_KING) {
        bb_attacked = KING_ATTACKS[to];
    }

    bb_attacked &= occ;

    // Swap loop
    for (int i = 0; i < 16; i++) {
        U64 w_atk = bb_attacked & b->white;
        U64 b_atk = bb_attacked & b->black;

        U64 attackers = attackers_side ? w_atk : b_atk;
        if (!attackers) break;

        // Find smallest attacker
        int best_sq = ctz(attackers);
        int best_piece = board_piece_at(b, best_sq);

        // Add new attacks from this piece
        if (best_piece == W_PAWN || best_piece == B_PAWN ||
            best_piece == W_BISHOP || best_piece == B_BISHOP ||
            best_piece == W_QUEEN || best_piece == B_QUEEN) {
            bb_attacked |= bb_bishop_attacks(to, occ);
        }
        if (best_piece == W_ROOK || best_piece == B_ROOK ||
            best_piece == W_QUEEN || best_piece == B_QUEEN) {
            bb_attacked |= bb_rook_attacks(to, occ);
        }

        bb_attacked ^= SQUARES[best_sq];
        occ ^= SQUARES[best_sq];
        bb_attacked &= occ;

        gain[n++] = s_piece_value[best_piece];
        attackers_side = !attackers_side;
    }

    // Alternating sum from the end
    for (int i = n - 1; i > 0; i--)
        gain[i-1] = -gain[i];
    return gain[0];
}

//=============================================================================
// Quiescence Search
//=============================================================================

int search_quiescence(Board *b, int alpha, int beta, SearchInfo *info, int ply) {
    info->nodes++;
    if (ply >= MAX_DEPTH - 1) return evaluate(b);
    if (is_repetition(b)) return ply - 30;
    if (is_fifty_moves(b)) return 0;

    // TT probe
    TTEntry tt_e;
    if (tt_probe(b->hash, &tt_e)) {
        int tt_score = value_from_tt(tt_e.score, ply);
        if (tt_e.flag == TT_FLAG_EXACT) return tt_score;
        if (tt_e.flag == TT_FLAG_ALPHA && tt_score <= alpha) alpha = tt_score;
        if (tt_e.flag == TT_FLAG_BETA && tt_score >= beta) beta = tt_score;
    }

    int stand_pat = evaluate(b);
    if (stand_pat >= beta) return beta;
    if (stand_pat > alpha) alpha = stand_pat;

    Movelist captures;
    generate_captures(b, &captures);
    score_moves_qsearch(&captures, b);

    int best = alpha;

    for (int i = 0; i < captures.count; i++) {
        pick_best(&captures, i);
        Move m = captures.moves[i];

        // Delta pruning
        int captured_val = s_piece_value[board_piece_at(b, m.to)];
        if (stand_pat + captured_val + 140 < alpha) continue;

        // SEE pruning
        if (see_capture(b, &m) < 0) continue;

        Board copy = *b;
        board_do_move(b, &m);
        if (board_in_check(b)) { *b = copy; continue; }

        int score = -search_quiescence(b, -beta, -alpha, info, ply + 1);
        *b = copy;

        if (score > best) best = score;
        if (best >= beta) return best;
        if (best > alpha) alpha = best;
    }

    return best;
}

//=============================================================================
// Main Alpha-Beta Search
//=============================================================================

int search_pv(Board *b, int depth, int alpha, int beta,
              SearchInfo *info, PV *pv, int ply, Move singular_move) {
    if (stop_flag) return 0;

    info->nodes++;
    if (ply > info->seldepth) info->seldepth = ply;
    if (ply >= MAX_DEPTH - 1) return evaluate(b);

    if (ply && is_repetition(b)) return ply - 30;
    if (ply && is_fifty_moves(b)) return 0;

    if (depth <= 0)
        return search_quiescence(b, alpha, beta, info, ply);

    bool in_check = board_in_check(b);
    bool is_pv = (beta > alpha + 1);

    if (in_check) depth++;

    // Mate distance pruning
    if (ply >= 6) {
        int mv = mate_from_score(ply);
        if (mv < beta) { beta = mv; if (alpha >= mv) return mv; }
        if (mv > alpha) { alpha = mv; if (beta <= mv) return mv; }
    }

    // TT probe
    TTEntry tt_e;
    Move tt_move = {0};
    bool tt_hit = tt_probe(b->hash, &tt_e);
    if (tt_hit) {
        tt_move = tt_e.move;
        int tt_score = value_from_tt(tt_e.score, ply);
        if (tt_e.depth >= depth) {
            if (tt_e.flag == TT_FLAG_EXACT) return tt_score;
            if (tt_e.flag == TT_FLAG_ALPHA && tt_score <= alpha) return alpha;
            if (tt_e.flag == TT_FLAG_BETA && tt_score >= beta) return beta;
        }
    }

    // IID: no TT move at sufficient depth
    if (!tt_hit && depth >= 6 && ply == 0) {
        PV iid_pv = {0};
        int iid_score = search_pv(b, depth - 2, -30000, 30000, info, &iid_pv, ply, (Move){0});
        if (abs(iid_score) < 29000) tt_move = iid_pv.pv[0];
    }

    // Razoring
    if (!in_check && depth <= 3 && !tt_hit) {
        int eval = evaluate(b);
        if (depth <= 1 && eval + 280 < alpha)
            return search_quiescence(b, alpha, beta, info, ply);
        if (depth == 2 && eval + 350 < alpha) {
            int s = search_quiescence(b, alpha, beta, info, ply);
            if (s < alpha) return s;
        }
    }

    // Null move
    if (!in_check && depth >= 3 && ply > 0 && !is_pv) {
        int R = depth > 6 ? 4 : 3;
        Board copy = *b;
        b->side = !b->side;
        if (b->ep_square >= 0) {
            b->hash ^= hash_ep[b->ep_square & 7];
            b->ep_square = -1;
        }
        int null_score = -search_pv(b, depth - 1 - R, -beta, -beta + 1, info, pv, ply + 1, (Move){0});
        *b = copy;
        if (null_score >= beta) return beta;
    }

    // Futility flag
    bool fl_futility = false;
    if (depth <= 6 && !in_check && !is_pv) {
        int eval = evaluate(b);
        if (eval + 50 + 50 * depth < beta) fl_futility = true;
    }

    // Generate and score moves
    Movelist ml;
    generate_moves(b, &ml);
    score_moves(&ml, b, &tt_move, depth, &s_history, &s_killers, ply);

    // Counter-move boost
    Move cm = ply > 0 ? s_prev_move[ply - 1] : (Move){0};
    if (cm.from || cm.to) {
        for (int i = 0; i < ml.count; i++) {
            if (ml.moves[i].from == cm.from && ml.moves[i].to == cm.to) {
                ml.moves[i].score += 15000;
                break;
            }
        }
    }

    sort_moves_insertion(&ml, 0);

    int legal = 0, quiet_tried = 0;
    int best_score = -30000;
    Move best_move = {0};
    PV best_child_pv;
    memset(&best_child_pv, 0, sizeof(best_child_pv));
    int alpha_orig = alpha;
    int score_type = TT_FLAG_ALPHA;
    bool is_capture = false, is_promo = false;

    for (int i = 0; i < ml.count; i++) {
        pick_best(&ml, i);
        Move m = ml.moves[i];
        is_capture = (m.flags & FLAG_CAPTURE) != 0;
        is_promo   = (m.flags & FLAG_PROMOTION) != 0;
        bool is_quiet  = !is_capture && !is_promo;

        Board copy = *b;
        board_do_move(b, &m);
        if (board_in_check(b)) { *b = copy; continue; }

        legal++;
        if (is_quiet) quiet_tried++;

        if (should_stop(info, b)) { *b = copy; return 0; }
        time_check(info, b);
        if (should_stop(info, b)) { *b = copy; return 0; }

        bool gives_check = board_in_check(b);

        // Futility pruning
        if (fl_futility && is_quiet && quiet_tried > 1) {
            *b = copy;
            continue;
        }

        // LMP (Late Move Pruning)
        if (is_quiet && depth <= 3 && quiet_tried > 4 * depth) {
            *b = copy;
            continue;
        }

        // Singular extension
        int new_depth = depth - 1;
        bool singular_ext = false;
        if (singular_move.from && singular_move.to &&
            m.from == singular_move.from && m.to == singular_move.to) {
            int s_score = search_pv(b, new_depth / 2, alpha, alpha + 1, info, pv, ply + 1, (Move){0});
            if (s_score < alpha - 80) {
                singular_ext = true;
                new_depth++;
            }
        }

        // LMR
        int reduction = 0;
        if (!singular_ext && depth >= 2 && legal > 3
            && !is_capture && !is_promo && !in_check && !gives_check) {
            reduction = calc_lmr(depth, legal, is_capture, is_promo, in_check, gives_check, is_pv);
        }

        int child_depth = new_depth - reduction;
        if (child_depth < 0) child_depth = 0;

        int score;
        PV child_pv2 = {0};

        if (legal == 1) {
            // Full window for first move (PVS)
            score = -search_pv(b, child_depth, -beta, -alpha, info, &child_pv2, ply + 1, (Move){0});
            if (reduction && score > alpha) {
                score = -search_pv(b, new_depth, -beta, -alpha, info, &child_pv2, ply + 1, (Move){0});
            }
        } else {
            // Null-window search
            score = -search_pv(b, child_depth, -alpha - 1, -alpha, info, &child_pv2, ply + 1, (Move){0});

            // Re-search if reduced and score beat alpha
            if (reduction && score > alpha) {
                PV child_pv3 = {0};
                int score2 = -search_pv(b, new_depth, -alpha - 1, -alpha, info, &child_pv3, ply + 1, (Move){0});
                if (score2 > score) {
                    child_pv2 = child_pv3;
                    score = score2;
                }
            }

            // Full window re-search if null-window failed to confirm
            if (score > alpha && score < beta && !singular_ext) {
                PV child_pv3 = {0};
                score = -search_pv(b, new_depth, -beta, -alpha, info, &child_pv3, ply + 1, (Move){0});
                if (score > best_score) {
                    best_score = score;
                    best_move = m;
                    best_child_pv = child_pv3;
                    pv->pv[ply] = m;
                    pv->pv_length[ply] = child_pv3.pv_length[ply + 1] + 1;
                    for (int j = ply + 1; j < child_pv3.pv_length[ply + 1] + 1; j++)
                        pv->pv[j] = child_pv3.pv[j];
                    *b = copy;
                    goto beta_cutoff;
                }
            }
        }

        *b = copy;

        if (score > best_score) {
            best_score = score;
            best_move = m;
            best_child_pv = child_pv2;
            pv->pv[ply] = m;
            pv->pv_length[ply] = child_pv2.pv_length[ply + 1] + 1;
            for (int j = ply + 1; j < child_pv2.pv_length[ply + 1] + 1; j++)
                pv->pv[j] = child_pv2.pv[j];
        }

beta_cutoff:
        if (best_score >= beta) {
            if (!is_capture && !is_promo) {
                killers_add(&s_killers, &m, ply);
                history_add_successful(&s_history, !b->side, &m, depth);
                counter_move_record(&m, ply);
            }
            tt_store(b->hash, &m, depth, value_to_tt(beta, ply), TT_FLAG_BETA);
            s_prev_move[ply] = m;
            return beta;
        }

        if (best_score > alpha) {
            alpha = best_score;
            score_type = TT_FLAG_EXACT;
        }
    }

    // Record previous move
    if (best_move.from || best_move.to) s_prev_move[ply] = best_move;

    if (legal == 0) {
        if (in_check) return mate_from_score(ply);
        return 0;
    }

    if (best_move.from || best_move.to) {
        tt_store(b->hash, &best_move, depth, value_to_tt(best_score, ply), score_type);
        if (!is_capture && !is_promo) {
            history_add_successful(&s_history, !b->side, &best_move, depth);
            counter_move_record(&best_move, ply);
        }
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
    memset(s_prev_move, 0, sizeof(s_prev_move));
    search_init_lmr();

    int completed_depth = 0;

    for (int depth = 1; depth <= max_depth && !stop_flag; depth++) {
        init_search_info(info, depth);

        int alpha = -30000, beta = 30000;

        // Aspiration windows
        if (depth >= 4 && completed_depth > 0) {
            int prev = s_best_score_this_iter;
            int window = 25 + abs(prev) / 40;
            alpha = max(prev - window, -30000);
            beta = min(prev + window, 30000);
        }

        PV pv = {0};
        int score = search_pv(b, depth, alpha, beta, info, &pv, 0, (Move){0});

        if (stop_flag) break;

        completed_depth = depth;
        s_best_score_this_iter = score;
        s_best_move_this_iter = pv.pv[0];

        int64_t elapsed = (int64_t)clock() * 1000 / CLOCKS_PER_SEC - info->time_start;
        int nps = elapsed > 0 ? (int)(info->nodes / (elapsed / 1000.0)) : 0;

        int score_show = abs(score) > 29000 ? mate_to_score(score) : score;

        printf("info depth %d seldepth %d nodes %lld time %lld nps %d score cp %d pv",
            depth, info->seldepth, (long long)info->nodes, (long long)elapsed, nps, score_show);
        for (int i = 0; i < 30 && pv.pv[i].from; i++) {
            char buf[8];
            move_to_uci(&pv.pv[i], buf, false);
            printf(" %s", buf);
        }
        printf("\n");
        fflush(stdout);
    }

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
