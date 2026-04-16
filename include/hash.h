#ifndef HASH_H
#define HASH_H

#include "types.h"

//=============================================================================
// Zobrist Hashing
//=============================================================================

void hash_init(void);
U64  hash_compute(const Board *b);
U64  hash_update(U64 key, Move *m, const Board *b);

// Hash keys (extern)
extern U64 hash_piece[13][64];
extern U64 hash_side;
extern U64 hash_ep[8];
extern U64 hash_castle[16];

//=============================================================================
// Transposition Table
//=============================================================================

void tt_init(int size_mb);
void tt_free(void);
void tt_clear(void);
void tt_age(void);

bool tt_probe(U64 key, TTEntry *e);
void tt_store(U64 key, Move *m, int depth, int score, int flag);
void tt_prefetch(U64 key);

int  tt_hashfull(void);
void tt_stats(uint64_t *hits, uint64_t *misses, uint64_t *writes);

// TT flags
#define TT_FLAG_ALPHA  0
#define TT_FLAG_EXACT  1
#define TT_FLAG_BETA   2

//=============================================================================
// RNG
//=============================================================================

U64 rng_rand(void);
U64 rng_srand(U64 seed);

#endif // HASH_H
