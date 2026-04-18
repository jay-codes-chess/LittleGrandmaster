#ifndef MOVES_H
#define MOVES_H

#include "types.h"
#include "board.h"

//=============================================================================
// Move List
//=============================================================================

void movelist_clear(Movelist *ml);
void movelist_add(Movelist *ml, Move *m);
Move *movelist_get(Movelist *ml, int i);

void move_copy(Move *dst, const Move *src);
bool move_equal(Move *a, Move *b);
bool move_is_capture(Move *m);
bool move_is_promotion(Move *m);
bool move_is_castle(Move *m);
bool move_is_en_passant(Move *m);

//=============================================================================
// Move Generation
//=============================================================================

// All pseudo-legal moves
void generate_moves(Board *b, Movelist *ml);

// Captures only
void generate_captures(Board *b, Movelist *ml);

// Quiets only (non-captures)
void generate_quiets(Board *b, Movelist *ml);

// Legal moves (filters illegal)
void generate_legal(Board *b, Movelist *ml);
void generate_legal_captures(Board *b, Movelist *ml);
void generate_legal_quiets(Board *b, Movelist *ml);

//=============================================================================
// Move Parsing / Formatting
//=============================================================================

Move move_from_uci(Board *b, const char *uci);
void move_to_uci(Move *m, char *buf, bool pretty);

Move move_from_san(Board *b, const char *san);
void move_to_san(Board *b, Move *m, char *buf);

//=============================================================================
// Move Ordering
//=============================================================================

void score_moves(Movelist *ml, Board *b, Move *tt_move, int depth, History *h, KillerTable *kt, int ply);
void score_moves_qsearch(Movelist *ml, Board *b);
void sort_moves(Movelist *ml, int start);
void sort_moves_insertion(Movelist *ml, int start);
int  pick_best(Movelist *ml, int start);

//=============================================================================
// Static Exchange Evaluation
//=============================================================================

int see(Board *b, Move *m);
int see_sign(Board *b, Move *m);  // returns 1 if >= 0, 0 if < 0

//=============================================================================
// Perft (for debugging)
//=============================================================================

uint64_t perft(Board *b, int depth);
uint64_t perft_divide(Board *b, int depth);

#endif // MOVES_H