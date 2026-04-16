#ifndef BOARD_H
#define BOARD_H

#include "types.h"

//=============================================================================
// Board Setup
//=============================================================================

void board_init(Board *b);
void board_reset(Board *b);
void board_load_fen(Board *b, const char *fen);
void board_to_fen(const Board *b, char *fen);
void board_print(const Board *b);

//=============================================================================
// Piece Access
//=============================================================================

int  board_piece_at(const Board *b, int sq);
void board_set_piece(Board *b, int sq, int piece);
void board_remove_piece(Board *b, int sq);
void board_add_piece(Board *b, int sq, int piece);

//=============================================================================
// Occupied Bitboards
//=============================================================================

U64 board_occupied(const Board *b);
U64 board_pieces(const Board *b, bool side);

//=============================================================================
// Attack Detection
//=============================================================================

U64 board_attackers_to(const Board *b, int sq, U64 occ);
U64 board_attackers_to_side(const Board *b, int sq, bool side, U64 occ);
bool board_is_attacked(const Board *b, int sq, bool by_side);
bool board_in_check(const Board *b);
int  board_checkers(const Board *b);

//=============================================================================
// Move Making
//=============================================================================

void board_do_move(Board *b, Move *m);
void board_undo_move(Board *b, Move *m);

// State management
void board_save_state(Board *b, BoardState *s);
void board_restore_state(Board *b, const BoardState *s);

// Legal move verification
bool board_legal(Board *b, Move *m);

// Castling rights
bool board_can_castle(const Board *b, int castle_right);

//=============================================================================
// Draw Detection
//=============================================================================

bool board_is_draw(const Board *b);
bool board_has_sufficient_material(const Board *b);

// Repetition
bool is_repetition(const Board *b);
bool is_fifty_moves(const Board *b);
bool is_insufficient(const Board *b);

//=============================================================================
// Board Utilities
//=============================================================================

int board_game_phase(const Board *b);
int board_material(const Board *b, bool side);
int board_imbalance(const Board *b);

// Mirror board for black
U64 mirror_square(int sq);

#endif // BOARD_H
