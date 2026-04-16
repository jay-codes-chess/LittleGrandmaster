#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hash.h"
#include "bitboard.h"

//=============================================================================
// Zobrist Hash Keys
//=============================================================================

U64 hash_piece[13][64];
U64 hash_side;
U64 hash_ep[8];
U64 hash_castle[16];

static U64 rng_state = 2463534242ULL;

static U64 rng_next(void) {
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 17;
    rng_state ^= rng_state << 5;
    return rng_state;
}

void hash_init(void) {
    rng_state = 2463534242ULL;
    for (int p = 1; p <= 12; p++)
        for (int s = 0; s < 64; s++)
            hash_piece[p][s] = rng_next();

    hash_side = rng_next();
    for (int i = 0; i < 8; i++) hash_ep[i] = rng_next();
    for (int i = 0; i < 16; i++) hash_castle[i] = rng_next();
}

U64 hash_compute(const Board *b) {
    U64 key = 0;

    // Pieces
    for (int p = 1; p <= 12; p++) {
        U64 bb = (p <= 6) ?
            ((p == 1) ? b->pawns : (p == 2) ? b->knights :
             (p == 3) ? b->bishops : (p == 4) ? b->rooks :
             (p == 5) ? b->queens : b->kings) & b->white :
            ((p == 7) ? b->pawns : (p == 8) ? b->knights :
             (p == 9) ? b->bishops : (p == 10) ? b->rooks :
             (p == 11) ? b->queens : b->kings) & b->black;

        while (bb) {
            int sq = ctz(bb);
            bb &= bb - 1;
            key ^= hash_piece[p][sq];
        }
    }

    // Side
    if (b->side) key ^= hash_side;

    // EP
    if (b->ep_square >= 0)
        key ^= hash_ep[FILE(b->ep_square)];

    // Castling
    key ^= hash_castle[b->castle];

    return key;
}

U64 hash_update(U64 key, Move *m, const Board *b) {
    (void)b;
    key ^= hash_piece[m->piece][m->from];
    key ^= hash_piece[m->piece][m->to];
    if (m->captured) key ^= hash_piece[m->captured][m->to];
    return key;
}

//=============================================================================
// Transposition Table
//=============================================================================

static TranspositionTable TT;

void tt_init(int size_mb) {
    tt_free();
    int entries = (size_mb * 1024 * 1024) / sizeof(TTEntry);
    TT.entries = (TTEntry *)calloc(entries, sizeof(TTEntry));
    TT.size = entries;
    TT.mask = entries - 1;
    TT.age = 0;
    TT.hits = TT.misses = TT.writes = 0;
}

void tt_free(void) {
    if (TT.entries) {
        free(TT.entries);
        TT.entries = NULL;
    }
}

void tt_clear(void) {
    if (TT.entries)
        memset(TT.entries, 0, TT.size * sizeof(TTEntry));
}

void tt_age(void) {
    TT.age = (TT.age + 1) & 7;
}

bool tt_probe(U64 key, TTEntry *e) {
    if (!TT.entries) return false;
    *e = TT.entries[key & TT.mask];
    if (e->key == key) return true;
    TT.misses++;
    return false;
}

void tt_store(U64 key, Move *m, int depth, int score, int flag) {
    if (!TT.entries) return;
    TTEntry *e = &TT.entries[key & TT.mask];
    e->key = key;
    e->move = *m;
    e->depth = depth;
    e->flag = flag;
    // Only store if depth >= existing, or age differs
    if (e->depth <= depth || e->age != TT.age) {
        e->score = score;
        e->age = TT.age;
    }
    TT.writes++;
}

void tt_prefetch(U64 key) {
    if (!TT.entries) return;
    // Prefetch is a hint - compilers handle this
    __builtin_prefetch(&TT.entries[key & TT.mask]);
}

int tt_hashfull(void) {
    if (!TT.entries) return 0;
    int count = 0;
    for (int i = 0; i < 1000; i++)
        if (TT.entries[(TT.mask - 999 + i) & TT.mask].key)
            count++;
    return count / 10;
}

void tt_stats(uint64_t *hits, uint64_t *misses, uint64_t *writes) {
    *hits = TT.hits;
    *misses = TT.misses;
    *writes = TT.writes;
}

//=============================================================================
// RNG
//=============================================================================

U64 rng_rand(void) {
    return rng_next();
}

U64 rng_srand(U64 seed) {
    rng_state = seed;
    return rng_next();
}
