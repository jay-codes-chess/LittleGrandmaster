#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "board.h"
#include "moves.h"
#include "search.h"
#include "eval.h"
#include "hash.h"
#include "uci.h"
#include "bitboard.h"

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    // Initialize
    bb_init();
    hash_init();
    eval_init();
    tt_init(256);

    // If given a FEN and depth, run perft
    if (argc >= 3 && strcmp(argv[1], "perft") == 0) {
        Board b;
        board_load_fen(&b, argv[2]);
        int depth = argc >= 4 ? atoi(argv[3]) : 5;
        int64_t start = clock();
        uint64_t n = perft(&b, depth);
        int64_t elapsed = (clock() - start) * 1000 / CLOCKS_PER_SEC;
        printf("nodes: %llu\ntime: %lld ms\nnps: %lld\n",
            (unsigned long long)n, (long long)elapsed,
            elapsed > 0 ? (long long)(n * 1000 / elapsed) : 0);
        tt_free();
        return 0;
    }

    // If given "bench", run benchmark
    if (argc >= 2 && strcmp(argv[1], "bench") == 0) {
        printf("Little Grandmaster Benchmark\n");
        printf("============================\n");

        const char *fen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
        Board b;
        board_load_fen(&b, fen);

        int64_t start = clock();
        uint64_t total = 0;
        for (int d = 1; d <= 6; d++) {
            uint64_t n = perft(&b, d);
            total += n;
            int64_t elapsed = (clock() - start);
            printf("depth %d: %10llu nodes  %8lld ms  %10lld nps\n",
                d, (unsigned long long)n,
                (long long)(elapsed * 1000 / CLOCKS_PER_SEC),
                (long long)(elapsed > 0 ? n * 1000 * CLOCKS_PER_SEC / elapsed : 0));
        }
        printf("total: %llu nodes\n", (unsigned long long)total);
        tt_free();
        return 0;
    }

    // UCI mode
    uci_loop();

    tt_free();
    return 0;
}
