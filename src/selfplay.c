//=============================================================================
// Self-Play Training Data Generator
// Plays N games using actual engine searches, records all positions with results.
// Output: FEN "result" lines for the Texel tuner.
//
// Usage: ./LittleGrandmaster selfplay [num_games=500] [depth=4] [output=training.epd]
//=============================================================================

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include "bitboard.h"
#include "board.h"
#include "moves.h"
#include "search.h"
#include "eval.h"
#include "hash.h"

// Per-game position storage
#define MAX_POS_PER_GAME 8192

static char  s_game_fen[MAX_POS_PER_GAME][256];
static int   s_game_count = 0;

// Output file handle
static FILE *s_out = NULL;
static int s_total_positions = 0;

static void flush_game_positions(float result) {
    for (int i = 0; i < s_game_count; i++) {
        const char *rstr;
        if (result > 0.75f) rstr = "1-0";
        else if (result < 0.25f) rstr = "0-1";
        else rstr = "1/2-1/2";
        fprintf(s_out, "%s \"%s\"\n", s_game_fen[i], rstr);
        s_total_positions++;
    }
    s_game_count = 0;
}

static void append_position(const Board *b) {
    if (s_game_count >= MAX_POS_PER_GAME) return;
    board_to_fen(b, s_game_fen[s_game_count]);
    s_game_count++;
}

static Move pick_move_search(Board *b, int depth, int movetime_ms) {
    Movelist ml;
    generate_moves(b, &ml);
    if (ml.count == 0) return (Move){0};
    if (ml.count == 1) return ml.moves[0];

    SearchInfo info = {0};
    info.time_limit = movetime_ms;
    info.stop = false;

    Board copy = *b;
    SearchResult r = search_start(&copy, &info, depth);
    *b = copy;

    if (r.found && (r.best_move.from || r.best_move.to)) {
        return r.best_move;
    }

    // Fallback: highest-static-eval move
    Movelist all;
    generate_moves(b, &all);
    int best_score = -100000;
    Move best = all.moves[0];
    Board tmp;
    for (int i = 0; i < all.count; i++) {
        tmp = *b;
        board_do_move(b, &all.moves[i]);
        if (board_in_check(b)) { *b = tmp; continue; }
        int sc = evaluate(b);
        if (sc > best_score) { best_score = sc; best = all.moves[i]; }
        *b = tmp;
    }
    return best;
}

static float play_one_game(int search_depth) {
    Board b;
    board_reset(&b);
    s_game_count = 0;

    for (int ply = 0; ply < 200; ply++) {
        Movelist ml;
        generate_moves(&b, &ml);
        if (ml.count == 0) break;
        if (is_repetition(&b)) break;
        if (is_fifty_moves(&b)) break;

        // Save position from move 8 onward
        if (ply >= 8) {
            append_position(&b);
        }

        // Pick and play move
        Move m = pick_move_search(&b, search_depth, 10);
        if (m.from == 0 && m.to == 0) break;
        board_do_move(&b, &m);
    }

    // Determine result
    float result;
    Movelist ml;
    generate_moves(&b, &ml);
    if (ml.count == 0) {
        if (board_in_check(&b)) {
            result = (b.side == 0) ? 0.0f : 1.0f;
        } else {
            result = 0.5f;
        }
    } else {
        result = 0.5f;
    }

    flush_game_positions(result);
    return result;
}

void selfplay_generate(int num_games, int depth, const char *output) {
    fprintf(stderr, "Self-play: %d games, depth %d, output %s\n",
            num_games, depth, output);
    fflush(stderr);

    bb_init();
    hash_init();
    eval_init();
    tt_init(64);

    s_out = fopen(output, "w");
    if (!s_out) {
        fprintf(stderr, "Cannot write %s\n", output);
        return;
    }

    int64_t start = clock();
    int ww = 0, bb = 0, dd = 0;

    for (int g = 0; g < num_games; g++) {
        float r = play_one_game(depth);
        if (r > 0.75f) ww++;
        else if (r < 0.25f) bb++;
        else dd++;

        if ((g + 1) % 100 == 0 || g == 0) {
            int64_t elapsed = (clock() - start) * 1000 / CLOCKS_PER_SEC;
            fprintf(stderr, "  Game %d/%d: [%dW %dB %dD] elapsed=%ldms total-pos=%d\n",
                g+1, num_games, ww, bb, dd, (long)elapsed, s_total_positions);
            fflush(stderr);
        }
    }

    fclose(s_out);
    s_out = NULL;

    int64_t elapsed = (clock() - start) * 1000 / CLOCKS_PER_SEC;
    fprintf(stderr, "\nDone: %d games, %d positions in %ld ms\n",
            num_games, s_total_positions, (long)elapsed);
    fprintf(stderr, "Written to: %s\n", output);
}