#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "board.h"
#include "bitboard.h"
#include "hash.h"

#define START_FEN "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"

//=============================================================================
// Piece Encoding Helpers
//=============================================================================

static int piece_char_to_piece(char c) {
    switch (c) {
        case 'P': return W_PAWN;
        case 'N': return W_KNIGHT;
        case 'B': return W_BISHOP;
        case 'R': return W_ROOK;
        case 'Q': return W_QUEEN;
        case 'K': return W_KING;
        case 'p': return B_PAWN;
        case 'n': return B_KNIGHT;
        case 'b': return B_BISHOP;
        case 'r': return B_ROOK;
        case 'q': return B_QUEEN;
        case 'k': return B_KING;
        default:  return EMPTY;
    }
}

static char piece_to_char(int piece) {
    return ".PNBRQKpnbrqk"[piece];
}

//=============================================================================
// Board Setup
//=============================================================================

void board_init(Board *b) {
    memset(b, 0, sizeof(Board));
    b->side = WHITE;
    b->ep_square = -1;
    b->castle = ALL_CASTLES;
    b->halfmove = 0;
    b->fullmove = 1;
    b->state_idx = 0;
    b->rep_count = 0;
}

void board_reset(Board *b) {
    board_load_fen(b, START_FEN);
}

void board_load_fen(Board *b, const char *fen) {
    board_init(b);

    int sq = A8;
    int i = 0;

    // Piece placement
    while (fen[i] && fen[i] != ' ') {
        char c = fen[i];
        if (c == '/') {
            // sq already correct - sq++ from last piece positioned us at next rank start
        } else if (c >= '1' && c <= '8') {
            sq += c - '0';
        } else {
            int piece = piece_char_to_piece(c);
            if (piece != EMPTY) {
                board_add_piece(b, sq, piece);
            }
            sq++;
        }
        i++;
    }
    while (fen[i] == ' ') i++;

    // Side
    b->side = (fen[i] == 'w') ? WHITE : BLACK;
    i += 2;
    while (fen[i] == ' ') i++;

    // Castling
    b->castle = 0;
    while (fen[i] && fen[i] != ' ') {
        switch (fen[i]) {
            case 'K': b->castle |= WK_CASTLE; break;
            case 'Q': b->castle |= WQ_CASTLE; break;
            case 'k': b->castle |= BK_CASTLE; break;
            case 'q': b->castle |= BQ_CASTLE; break;
        }
        i++;
    }
    i++;
    while (fen[i] == ' ') i++;

    // En passant
    if (fen[i] && fen[i] != '-') {
        int file = fen[i] - 'a';
        int rank = fen[i+1] - '1';
        b->ep_square = SQUARE(file, rank);
        i += 2;
    } else {
        b->ep_square = -1;
        if (fen[i] == '-') i++;
    }

    // Halfmove / fullmove
    while (fen[i] == ' ') i++;
    if (fen[i]) {
        sscanf(fen + i, "%d %d", &b->halfmove, &b->fullmove);
    }

    
    b->hash = hash_compute(b);
}

void board_to_fen(const Board *b, char *fen) {
    char *p = fen;
    for (int r = 7; r >= 0; r--) {
        int empty = 0;
        for (int f = 0; f < 8; f++) {
            int sq = SQUARE(f, r);
            int piece = board_piece_at(b, sq);
            if (piece == EMPTY) {
                empty++;
            } else {
                if (empty) { *p++ = '0' + empty; empty = 0; }
                *p++ = piece_to_char(piece);
            }
        }
        if (empty) *p++ = '0' + empty;
        if (r > 0) *p++ = '/';
    }
    *p++ = ' ';
    *p++ = b->side ? 'w' : 'b';
    *p++ = ' ';

    if (b->castle & WK_CASTLE) *p++ = 'K';
    if (b->castle & WQ_CASTLE) *p++ = 'Q';
    if (b->castle & BK_CASTLE) *p++ = 'k';
    if (b->castle & BQ_CASTLE) *p++ = 'q';
    if (!b->castle) *p++ = '-';
    *p++ = ' ';

    if (b->ep_square >= 0) {
        *p++ = 'a' + FILE(b->ep_square);
        *p++ = '1' + RANK(b->ep_square);
    } else {
        *p++ = '-';
    }
    *p++ = ' ';
    sprintf(p, "%d %d", b->halfmove, b->fullmove);
}

void board_print(const Board *b) {
    printf("\n");
    for (int r = 7; r >= 0; r--) {
        printf("%d  ", r + 1);
        for (int f = 0; f < 8; f++) {
            int sq = SQUARE(f, r);
            int piece = board_piece_at(b, sq);
            printf("%c ", piece_to_char(piece));
        }
        printf("\n");
    }
    printf("    a b c d e f g h\n\n");
    printf("Side: %s\n", b->side ? "WHITE" : "BLACK");
    printf("Castling: %c%c%c%c\n",
        (b->castle & WK_CASTLE) ? 'K' : '-',
        (b->castle & WQ_CASTLE) ? 'Q' : '-',
        (b->castle & BK_CASTLE) ? 'k' : '-',
        (b->castle & BQ_CASTLE) ? 'q' : '-');
    printf("EP: %d\n", b->ep_square);
    printf("Hash: %016llx\n", (unsigned long long)b->hash);
}

//=============================================================================
// Piece Access
//=============================================================================

int board_piece_at(const Board *b, int sq) {
    U64 mask = SQUARES[sq];
    if (b->white & mask) {
        if (b->pawns   & mask) return b->side == WHITE ? W_PAWN   : B_PAWN;
        if (b->knights & mask) return b->side == WHITE ? W_KNIGHT : B_KNIGHT;
        if (b->bishops & mask) return b->side == WHITE ? W_BISHOP : B_BISHOP;
        if (b->rooks   & mask) return b->side == WHITE ? W_ROOK   : B_ROOK;
        if (b->queens  & mask) return b->side == WHITE ? W_QUEEN  : B_QUEEN;
        if (b->kings   & mask) return b->side == WHITE ? W_KING   : B_KING;
    }
    if (b->black & mask) {
        if (b->pawns   & mask) return b->side == BLACK ? B_PAWN   : W_PAWN;
        if (b->knights & mask) return b->side == BLACK ? B_KNIGHT : W_KNIGHT;
        if (b->bishops & mask) return b->side == BLACK ? B_BISHOP : W_BISHOP;
        if (b->rooks   & mask) return b->side == BLACK ? B_ROOK   : W_ROOK;
        if (b->queens  & mask) return b->side == BLACK ? B_QUEEN  : W_QUEEN;
        if (b->kings   & mask) return b->side == BLACK ? B_KING   : W_KING;
    }
    return EMPTY;
}

void board_set_piece(Board *b, int sq, int piece) {
    board_remove_piece(b, sq);
    if (piece) board_add_piece(b, sq, piece);
}

void board_remove_piece(Board *b, int sq) {
    U64 mask = SQUARES[sq];
    b->white   &= ~mask;
    b->black   &= ~mask;
    b->pawns   &= ~mask;
    b->knights &= ~mask;
    b->bishops &= ~mask;
    b->rooks   &= ~mask;
    b->queens  &= ~mask;
    b->kings   &= ~mask;
}

void board_add_piece(Board *b, int sq, int piece) {
    U64 mask = SQUARES[sq];
    if (piece <= 6) b->white |= mask;
    else            b->black |= mask;

    switch (piece & 7) {
        case 1: b->pawns   |= mask; break;
        case 2: b->knights |= mask; break;
        case 3: b->bishops |= mask; break;
        case 4: b->rooks   |= mask; break;
        case 5: b->queens  |= mask; break;
        case 6: b->kings   |= mask; break;
    }
    
}

U64 board_occupied(const Board *b) {
    return b->white | b->black;
}

//=============================================================================
// Attack Detection
//=============================================================================

U64 board_attackers_to(const Board *b, int sq, U64 occ) {
    U64 attackers = 0;
    // Knights
    attackers |= KNIGHT_SPAN[sq] & b->knights;
    // Kings
    attackers |= KING_SPAN[sq] & b->kings;
    // Pawns
    attackers |= PAWN_ATTACKS_B[sq] & (b->pawns & b->white);
    attackers |= PAWN_ATTACKS_W[sq] & (b->pawns & b->black);
    // Sliders
    attackers |= bb_rook_attacks(sq, occ)   & (b->rooks | b->queens);
    attackers |= bb_bishop_attacks(sq, occ) & (b->bishops | b->queens);
    return attackers;
}

U64 board_attackers_to_side(const Board *b, int sq, bool side, U64 occ) {
    return board_attackers_to(b, sq, occ) & (side ? b->white : b->black);
}

bool board_is_attacked(const Board *b, int sq, bool by_side) {
    return board_attackers_to_side(b, sq, by_side, (b->white | b->black)) != 0;
}

bool board_in_check(const Board *b) {
    U64 king_bb = b->kings & (b->side ? b->white : b->black);
    if (!king_bb) return false;
    return board_is_attacked(b, ctz(king_bb), !b->side);
}

int board_checkers(const Board *b) {
    U64 king_bb = b->kings & (b->side ? b->white : b->black);
    if (!king_bb) return 64;
    U64 attackers = board_attackers_to(b, ctz(king_bb), (b->white | b->black));
    attackers &= b->side ? b->black : b->white;
    return attackers ? (int)ctz(attackers) : 64;
}

//=============================================================================
// State Management
//=============================================================================

void board_save_state(Board *b, BoardState *s) {
    s->hash     = b->hash;
    s->castle   = b->castle;
    s->ep_square = b->ep_square;
    s->halfmove = b->halfmove;
    s->checkers = board_checkers(b);
}

void board_restore_state(Board *b, const BoardState *s) {
    b->hash     = s->hash;
    b->castle   = s->castle;
    b->ep_square = s->ep_square;
    b->halfmove = s->halfmove;
    
}

bool board_can_castle(const Board *b, int cr) {
    return b->castle & cr;
}

//=============================================================================
// Move Making
//=============================================================================

void board_do_move(Board *b, Move *m) {
    // Save state
    if (b->state_idx < 255) {
        board_save_state(b, &b->state[b->state_idx]);
        b->state_idx++;
    }

    // Repetition
    if (b->rep_count < 255) {
        b->rep_keys[b->rep_count++] = b->hash;
    }

    int from = m->from, to = m->to;
    int piece = m->piece;
    int captured = m->captured;
    bool side = b->side;

    U64 from_bb = SQUARES[from];
    U64 to_bb = SQUARES[to];

    // Update hash: remove old, add new
    b->hash ^= hash_piece[piece][from];
    if (captured) b->hash ^= hash_piece[captured][to];

    // Remove piece from source
    switch (piece & 7) {
        case 1: b->pawns   &= ~from_bb; break;
        case 2: b->knights &= ~from_bb; break;
        case 3: b->bishops &= ~from_bb; break;
        case 4: b->rooks   &= ~from_bb; break;
        case 5: b->queens  &= ~from_bb; break;
        case 6: b->kings   &= ~from_bb; break;
    }

    // Handle EP capture
    if (m->flags & FLAG_EP_CAPTURE) {
        int ep_sq = side == WHITE ? to - 8 : to + 8;
        b->hash ^= hash_piece[side == WHITE ? B_PAWN : W_PAWN][ep_sq];
        board_remove_piece(b, ep_sq);
    }

    // Remove captured piece
    if (captured && !(m->flags & FLAG_EP_CAPTURE)) {
        switch (captured & 7) {
            case 1: b->pawns   &= ~to_bb; break;
            case 2: b->knights &= ~to_bb; break;
            case 3: b->bishops &= ~to_bb; break;
            case 4: b->rooks   &= ~to_bb; break;
            case 5: b->queens  &= ~to_bb; break;
            case 6: b->kings   &= ~to_bb; break;
        }
    }

    // Place piece on destination
    int place = m->promo ? m->promo : piece;
    switch (place & 7) {
        case 1: b->pawns   |= to_bb; break;
        case 2: b->knights |= to_bb; break;
        case 3: b->bishops |= to_bb; break;
        case 4: b->rooks   |= to_bb; break;
        case 5: b->queens  |= to_bb; break;
        case 6: b->kings   |= to_bb; break;
    }

    // Update color bitboards
    if (side == WHITE) {
        b->white &= ~from_bb;
        b->white |= to_bb;
    } else {
        b->black &= ~from_bb;
        b->black |= to_bb;
    }

    // Update hash with new piece
    b->hash ^= hash_piece[place][to];

    // Castling rook moves
    if (m->flags & FLAG_CASTLE) {
        int rank = side == WHITE ? 0 : 7;
        if (to == G1 || to == G8) {
            // Kingside
            board_remove_piece(b, H1 + rank * 56);
            board_add_piece(b, F1 + rank * 56, side == WHITE ? W_ROOK : B_ROOK);
            b->hash ^= hash_piece[W_ROOK][H1 + rank * 56];
            b->hash ^= hash_piece[W_ROOK][F1 + rank * 56];
        } else if (to == C1 || to == C8) {
            // Queenside
            board_remove_piece(b, A1 + rank * 56);
            board_add_piece(b, D1 + rank * 56, side == WHITE ? W_ROOK : B_ROOK);
            b->hash ^= hash_piece[W_ROOK][A1 + rank * 56];
            b->hash ^= hash_piece[W_ROOK][D1 + rank * 56];
        }
    }

    // Update castling rights
    if (b->castle) {
        if ((piece & 7) == 6) {
            b->castle &= side == WHITE ? ~(WK_CASTLE | WQ_CASTLE) : ~(BK_CASTLE | BQ_CASTLE);
        }
        if (from == H1 || to == H1) b->castle &= ~WK_CASTLE;
        if (from == A1 || to == A1) b->castle &= ~WQ_CASTLE;
        if (from == H8 || to == H8) b->castle &= ~BK_CASTLE;
        if (from == A8 || to == A8) b->castle &= ~BQ_CASTLE;
        b->hash ^= hash_castle[b->castle];
    }

    // Update EP square
    if (m->flags & FLAG_PAWN2) {
        int new_ep = side == WHITE ? to - 8 : to + 8;
        b->hash ^= hash_ep[new_ep & 7];
        b->ep_square = new_ep;
    } else {
        if (b->ep_square >= 0) {
            b->hash ^= hash_ep[b->ep_square & 7];
        }
        b->ep_square = -1;
    }

    // Toggle side
    b->side = !b->side;
    b->hash ^= hash_side;

    // Halfmove clock
    if ((piece & 7) == 1 || captured) b->halfmove = 0;
    else b->halfmove++;

    if (b->side == WHITE) b->fullmove++;

    
}

void board_undo_move(Board *b, Move *m) {
    if (b->state_idx > 0) b->state_idx--;
    if (b->rep_count > 0) b->rep_count--;

    bool side = !b->side;  // The side that made the move
    int from = m->from, to = m->to;
    int piece = m->piece;
    int captured = m->captured;
    U64 from_bb = SQUARES[from];
    U64 to_bb = SQUARES[to];

    // Undo castling rook
    if (m->flags & FLAG_CASTLE) {
        int rank = side == WHITE ? 0 : 7;
        if (to == G1 || to == G8) {
            board_remove_piece(b, F1 + rank * 56);
            board_add_piece(b, H1 + rank * 56, side == WHITE ? W_ROOK : B_ROOK);
        } else if (to == C1 || to == C8) {
            board_remove_piece(b, D1 + rank * 56);
            board_add_piece(b, A1 + rank * 56, side == WHITE ? W_ROOK : B_ROOK);
        }
    }

    // Undo EP capture
    if (m->flags & FLAG_EP_CAPTURE) {
        int ep_sq = side == WHITE ? to - 8 : to + 8;
        board_add_piece(b, ep_sq, side == WHITE ? B_PAWN : W_PAWN);
    }

    // Remove piece from destination
    int place = m->promo ? m->promo : piece;
    switch (place & 7) {
        case 1: b->pawns   &= ~to_bb; break;
        case 2: b->knights &= ~to_bb; break;
        case 3: b->bishops &= ~to_bb; break;
        case 4: b->rooks   &= ~to_bb; break;
        case 5: b->queens  &= ~to_bb; break;
        case 6: b->kings   &= ~to_bb; break;
    }

    // Place piece back at source
    switch (piece & 7) {
        case 1: b->pawns   |= from_bb; break;
        case 2: b->knights |= from_bb; break;
        case 3: b->bishops |= from_bb; break;
        case 4: b->rooks   |= from_bb; break;
        case 5: b->queens  |= from_bb; break;
        case 6: b->kings   |= from_bb; break;
    }

    // Restore captured piece
    if (captured && !(m->flags & FLAG_EP_CAPTURE)) {
        switch (captured & 7) {
            case 1: b->pawns   |= to_bb; break;
            case 2: b->knights |= to_bb; break;
            case 3: b->bishops |= to_bb; break;
            case 4: b->rooks   |= to_bb; break;
            case 5: b->queens  |= to_bb; break;
            case 6: b->kings   |= to_bb; break;
        }
    }

    // Update color bitboards
    if (side == WHITE) {
        b->white &= ~to_bb;
        b->white |= from_bb;
    } else {
        b->black &= ~to_bb;
        b->black |= from_bb;
    }

    // Toggle side back
    b->side = side;

    // Restore state
    board_restore_state(b, &b->state[b->state_idx]);
    
}

bool board_legal(Board *b, Move *m) {
    Board copy = *b;
    board_do_move(b, m);
    bool in_check = board_in_check(b);
    *b = copy;
    return !in_check;
}

//=============================================================================
// Draw Detection
//=============================================================================

bool is_repetition(const Board *b) {
    if (b->rep_count < 3) return false;
    U64 key = b->hash;
    int count = 0;
    for (int i = 0; i < b->rep_count; i++) {
        if (b->rep_keys[i] == key) {
            count++;
            if (count >= 2) return true;
        }
    }
    return false;
}

bool is_fifty_moves(const Board *b) {
    return b->halfmove >= 100;
}

bool is_insufficient(const Board *b) {
    // K vs K
    if (!b->pawns && !b->rooks && !b->queens && !b->knights && !b->bishops) return true;

    // K+B vs K or K+N vs K
    if (!b->pawns && !b->rooks && !b->queens && !b->knights) {
        U64 wb = b->bishops & b->white;
        U64 bb = b->bishops & b->black;
        if (!wb || !bb) return true;  // Same color bishops only
    }
    return false;
}

bool board_is_draw(const Board *b) {
    return is_repetition(b) || is_fifty_moves(b) || is_insufficient(b);
}

bool board_has_sufficient_material(const Board *b) {
    return !is_insufficient(b);
}

//=============================================================================
// Utilities
//=============================================================================

int board_game_phase(const Board *b) {
    int phase = 0;
    phase += popcount(b->pawns)   * 0;
    phase += popcount(b->knights) * PHASE_NN;
    phase += popcount(b->bishops) * PHASE_BISHOP;
    phase += popcount(b->rooks)   * PHASE_ROOK;
    phase += popcount(b->queens)  * PHASE_QUEEN;
    return min(phase, TOTAL_PHASE);
}

U64 mirror_square(int sq) {
    return SQUARES[sq ^ 56];
}
