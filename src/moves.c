#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "moves.h"
#include "board.h"
#include "bitboard.h"
#include "search.h"

//=============================================================================
// Move List
//=============================================================================

void movelist_clear(Movelist *ml) { ml->count = 0; }

void movelist_add(Movelist *ml, Move *m) {
    if (ml->count < MAX_MOVES) ml->moves[ml->count++] = *m;
}

Move *movelist_get(Movelist *ml, int i) { return &ml->moves[i]; }

void move_copy(Move *dst, const Move *src) { *dst = *src; }

bool move_equal(Move *a, Move *b) {
    return a->from == b->from && a->to == b->to;
}

bool move_is_capture(Move *m)     { return m->flags & FLAG_CAPTURE; }
bool move_is_promotion(Move *m)   { return m->flags & FLAG_PROMOTION; }
bool move_is_castle(Move *m)      { return m->flags & FLAG_CASTLE; }
bool move_is_en_passant(Move *m)  { return m->flags & FLAG_EP_CAPTURE; }

//=============================================================================
// Move Generation - Helpers
//=============================================================================

static void add_pawn_move(const Board *b, Movelist *ml, int from, int to, int flags) {
    int piece = b->side ? W_PAWN : B_PAWN;
    if (RANK(to) == 7 || RANK(to) == 0) {
        movelist_add(ml, &(Move){from, to, piece, EMPTY, W_QUEEN,  flags | FLAG_PROMOTION});
        movelist_add(ml, &(Move){from, to, piece, EMPTY, W_ROOK,   flags | FLAG_PROMOTION});
        movelist_add(ml, &(Move){from, to, piece, EMPTY, W_BISHOP, flags | FLAG_PROMOTION});
        movelist_add(ml, &(Move){from, to, piece, EMPTY, W_KNIGHT, flags | FLAG_PROMOTION});
    } else {
        movelist_add(ml, &(Move){from, to, piece, EMPTY, 0, flags});
    }
}

static void add_capture(const Board *b, Movelist *ml, int from, int to, int flags) {
    int piece = b->side ? W_PAWN : B_PAWN;
    int captured = board_piece_at(b, to);
    if (RANK(to) == 7 || RANK(to) == 0) {
        movelist_add(ml, &(Move){from, to, piece, captured, W_QUEEN,  flags | FLAG_CAPTURE | FLAG_PROMOTION});
        movelist_add(ml, &(Move){from, to, piece, captured, W_ROOK,   flags | FLAG_CAPTURE | FLAG_PROMOTION});
        movelist_add(ml, &(Move){from, to, piece, captured, W_BISHOP, flags | FLAG_CAPTURE | FLAG_PROMOTION});
        movelist_add(ml, &(Move){from, to, piece, captured, W_KNIGHT, flags | FLAG_CAPTURE | FLAG_PROMOTION});
    } else {
        movelist_add(ml, &(Move){from, to, piece, captured, 0, flags | FLAG_CAPTURE});
    }
}

//=============================================================================
// Pseudo-Legal Move Generation
//=============================================================================

void generate_moves(Board *b, Movelist *ml) {
    movelist_clear(ml);
    bool side = b->side;
    U64 us = side ? b->white : b->black;
    U64 them = side ? b->black : b->white;
    U64 occ = (b->white | b->black);

    // Pawns
    U64 pawns = b->pawns & us;
    while (pawns) {
        int sq = pop_lsb(&pawns);
        int r = RANK(sq);

        if (side == WHITE) {
            int to = sq + 8;
            if (!(occ & SQUARES[to])) {
                add_pawn_move(b, ml, sq, to, 0);
                if (r == 1 && !(occ & SQUARES[to + 8]))
                    movelist_add(ml, &(Move){sq, to + 8, W_PAWN, EMPTY, 0, FLAG_PAWN2});
            }
            U64 atk = PAWN_ATTACKS_W[sq] & them;
            while (atk) add_capture(b, ml, sq, pop_lsb(&atk), 0);
            if (b->ep_square >= 0 && (PAWN_ATTACKS_W[sq] & SQUARES[b->ep_square]))
                movelist_add(ml, &(Move){sq, b->ep_square, W_PAWN, B_PAWN, 0, FLAG_CAPTURE | FLAG_EP_CAPTURE});
        } else {
            int to = sq - 8;
            if (!(occ & SQUARES[to])) {
                add_pawn_move(b, ml, sq, to, 0);
                if (r == 6 && !(occ & SQUARES[to - 8]))
                    movelist_add(ml, &(Move){sq, to - 8, B_PAWN, EMPTY, 0, FLAG_PAWN2});
            }
            U64 atk = PAWN_ATTACKS_B[sq] & them;
            while (atk) add_capture(b, ml, sq, pop_lsb(&atk), 0);
            if (b->ep_square >= 0 && (PAWN_ATTACKS_B[sq] & SQUARES[b->ep_square]))
                movelist_add(ml, &(Move){sq, b->ep_square, B_PAWN, W_PAWN, 0, FLAG_CAPTURE | FLAG_EP_CAPTURE});
        }
    }

    // Knights
    U64 knights = b->knights & us;
    while (knights) {
        int sq = pop_lsb(&knights);
        U64 atk = KNIGHT_SPAN[sq] & ~us;
        while (atk) {
            int to = pop_lsb(&atk);
            movelist_add(ml, &(Move){sq, to, side ? W_KNIGHT : B_KNIGHT,
                board_piece_at(b, to), 0, board_piece_at(b, to) ? FLAG_CAPTURE : 0});
        }
    }

    // Bishops + Queens
    U64 bishops = (b->bishops | b->queens) & us;
    while (bishops) {
        int sq = pop_lsb(&bishops);
        bool is_queen = b->queens & SQUARES[sq];
        U64 atk = bb_bishop_attacks(sq, occ) & ~us;
        while (atk) {
            int to = pop_lsb(&atk);
            movelist_add(ml, &(Move){sq, to, is_queen ? (side ? W_QUEEN : B_QUEEN) : (side ? W_BISHOP : B_BISHOP),
                board_piece_at(b, to), 0, board_piece_at(b, to) ? FLAG_CAPTURE : 0});
        }
    }

    // Rooks + Queens
    U64 rooks = (b->rooks | b->queens) & us;
    while (rooks) {
        int sq = pop_lsb(&rooks);
        bool is_queen = b->queens & SQUARES[sq];
        U64 atk = bb_rook_attacks(sq, occ) & ~us;
        while (atk) {
            int to = pop_lsb(&atk);
            movelist_add(ml, &(Move){sq, to, is_queen ? (side ? W_QUEEN : B_QUEEN) : (side ? W_ROOK : B_ROOK),
                board_piece_at(b, to), 0, board_piece_at(b, to) ? FLAG_CAPTURE : 0});
        }
    }

    // King
    U64 king_bb = b->kings & us;
    if (king_bb) {
        int sq = ctz(king_bb);
        U64 atk = KING_SPAN[sq] & ~us;
        while (atk) {
            int to = pop_lsb(&atk);
            movelist_add(ml, &(Move){sq, to, side ? W_KING : B_KING,
                board_piece_at(b, to), 0, board_piece_at(b, to) ? FLAG_CAPTURE : 0});
        }
        // Castling
        if (side == WHITE) {
            if ((b->castle & WK_CASTLE) && !(occ & (SQUARES[F1] | SQUARES[G1]))
                && !board_is_attacked(b, E1, BLACK) && !board_is_attacked(b, F1, BLACK))
                movelist_add(ml, &(Move){E1, G1, W_KING, EMPTY, 0, FLAG_CASTLE});
            if ((b->castle & WQ_CASTLE) && !(occ & (SQUARES[B1] | SQUARES[C1] | SQUARES[D1]))
                && !board_is_attacked(b, E1, BLACK) && !board_is_attacked(b, D1, BLACK))
                movelist_add(ml, &(Move){E1, C1, W_KING, EMPTY, 0, FLAG_CASTLE});
        } else {
            if ((b->castle & BK_CASTLE) && !(occ & (SQUARES[F8] | SQUARES[G8]))
                && !board_is_attacked(b, E8, WHITE) && !board_is_attacked(b, F8, WHITE))
                movelist_add(ml, &(Move){E8, G8, B_KING, EMPTY, 0, FLAG_CASTLE});
            if ((b->castle & BQ_CASTLE) && !(occ & (SQUARES[B8] | SQUARES[C8] | SQUARES[D8]))
                && !board_is_attacked(b, E8, WHITE) && !board_is_attacked(b, D8, WHITE))
                movelist_add(ml, &(Move){E8, C8, B_KING, EMPTY, 0, FLAG_CASTLE});
        }
    }
}

void generate_captures(Board *b, Movelist *ml) {
    movelist_clear(ml);
    bool side = b->side;
    U64 us = side ? b->white : b->black;
    U64 them = side ? b->black : b->white;
    U64 occ = (b->white | b->black);

    U64 pawns = b->pawns & us;
    while (pawns) {
        int sq = pop_lsb(&pawns);
        if (side == WHITE) {
            U64 atk = PAWN_ATTACKS_W[sq] & them;
            while (atk) add_capture(b, ml, sq, pop_lsb(&atk), 0);
            if (b->ep_square >= 0 && (PAWN_ATTACKS_W[sq] & SQUARES[b->ep_square]))
                movelist_add(ml, &(Move){sq, b->ep_square, W_PAWN, B_PAWN, 0, FLAG_CAPTURE | FLAG_EP_CAPTURE});
        } else {
            U64 atk = PAWN_ATTACKS_B[sq] & them;
            while (atk) add_capture(b, ml, sq, pop_lsb(&atk), 0);
            if (b->ep_square >= 0 && (PAWN_ATTACKS_B[sq] & SQUARES[b->ep_square]))
                movelist_add(ml, &(Move){sq, b->ep_square, B_PAWN, W_PAWN, 0, FLAG_CAPTURE | FLAG_EP_CAPTURE});
        }
    }

    U64 knights = b->knights & us;
    while (knights) {
        int sq = pop_lsb(&knights);
        U64 atk = KNIGHT_SPAN[sq] & them;
        while (atk) movelist_add(ml, &(Move){sq, pop_lsb(&atk), side ? W_KNIGHT : B_KNIGHT,
            board_piece_at(b, (int)ctz(atk)), 0, FLAG_CAPTURE});
    }

    U64 bishops = b->bishops & us;
    while (bishops) {
        int sq = pop_lsb(&bishops);
        U64 atk = bb_bishop_attacks(sq, occ) & them;
        while (atk) movelist_add(ml, &(Move){sq, pop_lsb(&atk), side ? W_BISHOP : B_BISHOP,
            board_piece_at(b, (int)ctz(atk)), 0, FLAG_CAPTURE});
    }

    U64 rooks = b->rooks & us;
    while (rooks) {
        int sq = pop_lsb(&rooks);
        U64 atk = bb_rook_attacks(sq, occ) & them;
        while (atk) movelist_add(ml, &(Move){sq, pop_lsb(&atk), side ? W_ROOK : B_ROOK,
            board_piece_at(b, (int)ctz(atk)), 0, FLAG_CAPTURE});
    }

    U64 queens = b->queens & us;
    while (queens) {
        int sq = pop_lsb(&queens);
        U64 atk = (bb_rook_attacks(sq, occ) | bb_bishop_attacks(sq, occ)) & them;
        while (atk) movelist_add(ml, &(Move){sq, pop_lsb(&atk), side ? W_QUEEN : B_QUEEN,
            board_piece_at(b, (int)ctz(atk)), 0, FLAG_CAPTURE});
    }

    U64 king_bb = b->kings & us;
    if (king_bb) {
        int sq = ctz(king_bb);
        U64 atk = KING_SPAN[sq] & them;
        while (atk) movelist_add(ml, &(Move){sq, pop_lsb(&atk), side ? W_KING : B_KING,
            board_piece_at(b, (int)ctz(atk)), 0, FLAG_CAPTURE});
    }
}

void generate_quiets(Board *b, Movelist *ml) {
    movelist_clear(ml);
    bool side = b->side;
    U64 us = side ? b->white : b->black;
    U64 occ = (b->white | b->black);
    U64 not_occ = ~occ;

    U64 pawns = b->pawns & us;
    while (pawns) {
        int sq = pop_lsb(&pawns);
        int r = RANK(sq);
        if (side == WHITE) {
            int to = sq + 8;
            if (!(occ & SQUARES[to])) {
                movelist_add(ml, &(Move){sq, to, W_PAWN, EMPTY, 0, 0});
                if (r == 1 && !(occ & SQUARES[to + 8]))
                    movelist_add(ml, &(Move){sq, to + 8, W_PAWN, EMPTY, 0, FLAG_PAWN2});
            }
        } else {
            int to = sq - 8;
            if (!(occ & SQUARES[to])) {
                movelist_add(ml, &(Move){sq, to, B_PAWN, EMPTY, 0, 0});
                if (r == 6 && !(occ & SQUARES[to - 8]))
                    movelist_add(ml, &(Move){sq, to - 8, B_PAWN, EMPTY, 0, FLAG_PAWN2});
            }
        }
    }

    U64 knights = b->knights & us;
    while (knights) {
        int sq = pop_lsb(&knights);
        U64 atk = KNIGHT_SPAN[sq] & not_occ;
        while (atk) movelist_add(ml, &(Move){sq, pop_lsb(&atk), side ? W_KNIGHT : B_KNIGHT, EMPTY, 0, 0});
    }

    U64 bishops = b->bishops & us;
    while (bishops) {
        int sq = pop_lsb(&bishops);
        U64 atk = bb_bishop_attacks(sq, occ) & not_occ;
        while (atk) movelist_add(ml, &(Move){sq, pop_lsb(&atk), side ? W_BISHOP : B_BISHOP, EMPTY, 0, 0});
    }

    U64 rooks = b->rooks & us;
    while (rooks) {
        int sq = pop_lsb(&rooks);
        U64 atk = bb_rook_attacks(sq, occ) & not_occ;
        while (atk) movelist_add(ml, &(Move){sq, pop_lsb(&atk), side ? W_ROOK : B_ROOK, EMPTY, 0, 0});
    }

    U64 queens = b->queens & us;
    while (queens) {
        int sq = pop_lsb(&queens);
        U64 atk = (bb_rook_attacks(sq, occ) | bb_bishop_attacks(sq, occ)) & not_occ;
        while (atk) movelist_add(ml, &(Move){sq, pop_lsb(&atk), side ? W_QUEEN : B_QUEEN, EMPTY, 0, 0});
    }

    U64 king_bb = b->kings & us;
    if (king_bb) {
        int sq = ctz(king_bb);
        U64 atk = KING_SPAN[sq] & not_occ;
        while (atk) movelist_add(ml, &(Move){sq, pop_lsb(&atk), side ? W_KING : B_KING, EMPTY, 0, 0});
        // Castling
        if (side == WHITE) {
            if ((b->castle & WK_CASTLE) && !(occ & (SQUARES[F1] | SQUARES[G1]))
                && !board_is_attacked(b, E1, BLACK) && !board_is_attacked(b, F1, BLACK))
                movelist_add(ml, &(Move){E1, G1, W_KING, EMPTY, 0, FLAG_CASTLE});
            if ((b->castle & WQ_CASTLE) && !(occ & (SQUARES[B1] | SQUARES[C1] | SQUARES[D1]))
                && !board_is_attacked(b, E1, BLACK) && !board_is_attacked(b, D1, BLACK))
                movelist_add(ml, &(Move){E1, C1, W_KING, EMPTY, 0, FLAG_CASTLE});
        } else {
            if ((b->castle & BK_CASTLE) && !(occ & (SQUARES[F8] | SQUARES[G8]))
                && !board_is_attacked(b, E8, WHITE) && !board_is_attacked(b, F8, WHITE))
                movelist_add(ml, &(Move){E8, G8, B_KING, EMPTY, 0, FLAG_CASTLE});
            if ((b->castle & BQ_CASTLE) && !(occ & (SQUARES[B8] | SQUARES[C8] | SQUARES[D8]))
                && !board_is_attacked(b, E8, WHITE) && !board_is_attacked(b, D8, WHITE))
                movelist_add(ml, &(Move){E8, C8, B_KING, EMPTY, 0, FLAG_CASTLE});
        }
    }
}

//=============================================================================
// Legal Move Generation
//=============================================================================

void generate_legal(Board *b, Movelist *ml) {
    Movelist pseudo;
    generate_moves(b, &pseudo);
    ml->count = 0;
    for (int i = 0; i < pseudo.count; i++) {
        Move m = pseudo.moves[i];
        Board copy = *b;
        board_do_move(b, &m);
        if (!board_in_check(b))
            ml->moves[ml->count++] = m;
        *b = copy;
    }
}

void generate_legal_captures(Board *b, Movelist *ml) {
    Movelist pseudo;
    generate_captures(b, &pseudo);
    ml->count = 0;
    for (int i = 0; i < pseudo.count; i++) {
        Move m = pseudo.moves[i];
        Board copy = *b;
        board_do_move(&copy, &m);
        if (!board_in_check(&copy))
            ml->moves[ml->count++] = m;
    }
}

void generate_legal_quiets(Board *b, Movelist *ml) {
    Movelist pseudo;
    generate_quiets(b, &pseudo);
    ml->count = 0;
    for (int i = 0; i < pseudo.count; i++) {
        Move m = pseudo.moves[i];
        Board copy = *b;
        board_do_move(&copy, &m);
        if (!board_in_check(&copy))
            ml->moves[ml->count++] = m;
    }
}

//=============================================================================
// Move Parsing / Formatting
//=============================================================================

Move move_from_uci(Board *b, const char *uci) {
    if (strlen(uci) < 4) return (Move){0, 0, 0, 0, 0, 0};
    int from = (uci[0] - 'a') + (uci[1] - '1') * 8;
    int to = (uci[2] - 'a') + (uci[3] - '1') * 8;
    int promo = 0;
    if (strlen(uci) >= 5) {
        switch (uci[4]) {
            case 'q': case 'Q': promo = W_QUEEN; break;
            case 'r': case 'R': promo = W_ROOK; break;
            case 'b': case 'B': promo = W_BISHOP; break;
            case 'n': case 'N': promo = W_KNIGHT; break;
        }
    }
    return (Move){from, to, board_piece_at(b, from), board_piece_at(b, to), promo, 0};
}

void move_to_uci(Move *m, char *buf, bool pretty) {
    (void)pretty;
    sprintf(buf, "%c%d%c%d",
        'a' + FILE(m->from), 1 + RANK(m->from),
        'a' + FILE(m->to), 1 + RANK(m->to));
    if (m->promo) {
        buf[4] = "qrbn"[m->promo - 1];
        buf[5] = '\0';
    }
}

Move move_from_san(Board *b, const char *san) {
    Movelist ml;
    generate_legal(b, &ml);

    // Castling
    if (!strcmp(san, "O-O") || !strcmp(san, "0-0"))
        for (int i = 0; i < ml.count; i++)
            if ((ml.moves[i].flags & FLAG_CASTLE) && (ml.moves[i].to == G1 || ml.moves[i].to == G8))
                return ml.moves[i];
    if (!strcmp(san, "O-O-O") || !strcmp(san, "0-0-0"))
        for (int i = 0; i < ml.count; i++)
            if ((ml.moves[i].flags & FLAG_CASTLE) && (ml.moves[i].to == C1 || ml.moves[i].to == C8))
                return ml.moves[i];

    for (int i = 0; i < ml.count; i++) {
        char buf[6];
        move_to_uci(&ml.moves[i], buf, false);
        if (!strncmp(san, buf, 4)) {
            if (strlen(san) == 5 && (san[4] == 'q' || san[4] == 'r' || san[4] == 'b' || san[4] == 'n')) {
                if (ml.moves[i].promo) return ml.moves[i];
            } else if (strlen(san) == 4 || strlen(san) == 5) {
                return ml.moves[i];
            }
        }
    }
    return (Move){0, 0, 0, 0, 0, 0};
}

void move_to_san(Board *b, Move *m, char *buf) {
    if (m->flags & FLAG_CASTLE) {
        sprintf(buf, "%s", m->to > m->from ? "O-O" : "O-O-O");
        return;
    }

    char piece_char = ".PNBRQK"[PT(m->piece)];
    char to_buf[8];
    move_to_uci(m, to_buf, false);

    if ((PT(m->piece)) == 1) {  // Pawn
        if (m->flags & FLAG_CAPTURE) {
            sprintf(buf, "%cx%s", 'a' + FILE(m->from), to_buf + 2);
        } else {
            sprintf(buf, "%s", to_buf + 2);
        }
    } else {
        sprintf(buf, "%c%s", piece_char, to_buf + 2);
    }

    if (m->promo) {
        sprintf(buf + strlen(buf), "=%c", "QRBN"[m->promo - 1]);
    }
}

//=============================================================================
// Move Ordering
//=============================================================================

static const int MVV_LVA_VICTIM[7] = {0, 100, 200, 300, 400, 500, 600};
static const int MVV_LVA_ATTACKER[7] = {0, 10, 20, 30, 40, 50, 60};

void score_moves(Movelist *ml, Board *b, Move *tt_move, int depth, History *h, KillerTable *kt, int ply) {
    for (int i = 0; i < ml->count; i++) {
        Move *m = &ml->moves[i];
        int score = 0;


        if (tt_move && move_equal(m, tt_move)) {
            score = 200000;
        } else if (m->flags & FLAG_CAPTURE) {
            score = MVV_LVA_VICTIM[PT(m->captured)] - MVV_LVA_ATTACKER[PT(m->piece)];
            score += 100000;
        } else {
            // History score for quiet moves
            score = history_score(h, b->side, m);
            // Killer move bonus
            if (is_killer(kt, m, ply)) score += 5000;
        }
        m->score = score;
    }
}

void score_moves_qsearch(Movelist *ml, Board *b) {
    (void)b;
    for (int i = 0; i < ml->count; i++) {
        Move *m = &ml->moves[i];
        m->score = (m->flags & FLAG_CAPTURE) ?
            MVV_LVA_VICTIM[PT(m->captured)] - MVV_LVA_ATTACKER[PT(m->piece)] : 0;
    }
}

void sort_moves(Movelist *ml, int start) {
    for (int i = start; i < ml->count - 1; i++)
        for (int j = i + 1; j < ml->count; j++)
            if (ml->moves[j].score > ml->moves[i].score) {
                Move tmp = ml->moves[i]; ml->moves[i] = ml->moves[j]; ml->moves[j] = tmp;
            }
}

void sort_moves_insertion(Movelist *ml, int start) {
    for (int i = start + 1; i < ml->count; i++) {
        Move m = ml->moves[i];
        int j = i;
        while (j > start && ml->moves[j-1].score < m.score) {
            ml->moves[j] = ml->moves[j-1];
            j--;
        }
        ml->moves[j] = m;
    }
}

int pick_best(Movelist *ml, int start) {
    int best = start;
    for (int i = start + 1; i < ml->count; i++)
        if (ml->moves[i].score > ml->moves[best].score)
            best = i;
    Move tmp = ml->moves[start]; ml->moves[start] = ml->moves[best]; ml->moves[best] = tmp;
    return start;
}

//=============================================================================
// Static Exchange Evaluation
//=============================================================================

static const int SEE_VALS[7] = {0, 1, 3, 3, 5, 9, 100};

int see(Board *b, Move *m) {
    if (m->flags & FLAG_CASTLE) return 0;

    int from = m->from, to = m->to;
    int piece = m->piece;
    int captured = m->captured;

    U64 occ = (b->white | b->black);
    occ &= ~SQUARES[to];
    occ |= SQUARES[from];

    if ((PT(piece)) == 1 && to == b->ep_square) {
        occ &= ~SQUARES[b->side == WHITE ? to - 8 : to + 8];
    }

    int gain[32];
    int n = 0;
    gain[n++] = SEE_VALS[PT(captured)];

    bool side = !b->side;
    U64 attackers = board_attackers_to(b, to, occ) & occ;
    U64 stm_attackers;

    while ((stm_attackers = attackers & (side ? b->white : b->black))) {
        // Find least valuable piece
        int sq;
        U64 stm_bb = stm_attackers;

        // Pawns
        if (b->pawns & stm_bb) {
            sq = ctz(b->pawns & stm_bb);
            gain[n++] = SEE_VALS[1];
            occ &= ~SQUARES[sq];
            if (RANK(sq) == 7 || RANK(sq) == 0) {
                occ |= SQUARES[sq];  // Promotion doesn't give material for SEE
            }
        }
        // Knights
        else if (b->knights & stm_bb) {
            sq = ctz(b->knights & stm_bb);
            gain[n++] = SEE_VALS[2];
            occ &= ~SQUARES[sq];
        }
        // Bishops
        else if (b->bishops & stm_bb) {
            sq = ctz(b->bishops & stm_bb);
            gain[n++] = SEE_VALS[3];
            occ &= ~SQUARES[sq];
            occ |= bb_bishop_attacks(sq, occ) & (b->bishops | b->queens);
        }
        // Rooks
        else if (b->rooks & stm_bb) {
            sq = ctz(b->rooks & stm_bb);
            gain[n++] = SEE_VALS[4];
            occ &= ~SQUARES[sq];
            occ |= bb_rook_attacks(sq, occ) & (b->rooks | b->queens);
        }
        // Queens
        else if (b->queens & stm_bb) {
            sq = ctz(b->queens & stm_bb);
            gain[n++] = SEE_VALS[5];
            occ &= ~SQUARES[sq];
            occ |= (bb_rook_attacks(sq, occ) | bb_bishop_attacks(sq, occ)) & (b->queens | b->rooks | b->bishops);
        }
        // King
        else if (b->kings & stm_bb) {
            sq = ctz(b->kings & stm_bb);
            gain[n++] = SEE_VALS[6];
            occ &= ~SQUARES[sq];
        } else {
            break;
        }

        side = !side;
        attackers = board_attackers_to(b, to, occ) & occ;
    }

    // Alternating sum
    for (int i = n - 1; i > 0; i--)
        gain[i-1] = -max(gain[i-1] - gain[i], 0);

    return gain[0];
}

int see_sign(Board *b, Move *m) {
    return see(b, m) >= 0;
}

//=============================================================================
// Perft
//=============================================================================

static uint64_t perft_rec(Board *b, int depth) {
    if (depth == 0) return 1;
    Movelist ml;
    generate_legal(b, &ml);    if (depth == 1) return ml.count;

    uint64_t nodes = 0;
    for (int i = 0; i < ml.count; i++) {
        Board copy = *b;
        board_do_move(b, &ml.moves[i]);
        nodes += perft_rec(b, depth - 1);
        *b = copy;
    }
    return nodes;
}

uint64_t perft(Board *b, int depth) {
    return perft_rec(b, depth);
}

uint64_t perft_divide(Board *b, int depth) {
    Movelist ml;
    generate_legal(b, &ml);
    uint64_t total = 0;
    printf("\nperft(%d):\n", depth);
    for (int i = 0; i < ml.count; i++) {
        char buf[8];
        move_to_uci(&ml.moves[i], buf, false);
        Board copy = *b;
        board_do_move(b, &ml.moves[i]);
        uint64_t n = perft_rec(b, depth - 1);
        printf("  %s: %llu\n", buf, (unsigned long long)n);
        total += n;
        *b = copy;
    }
    printf("total: %llu\n", (unsigned long long)total);
    return total;
}
