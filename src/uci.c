#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "uci.h"
#include "board.h"

extern volatile bool stop_flag;
#include "moves.h"
#include "search.h"
#include "eval.h"
#include "hash.h"
#include "bitboard.h"
#include "tuner.h"

//=============================================================================
// UCI Input Line
//=============================================================================

static char input_line[8192];

static char *next_token(char **line) {
    while (**line && (**line == ' ' || **line == '\n')) (*line)++;
    char *start = *line;
    while (**line && **line != ' ' && **line != '\n') (*line)++;
    if (**line) { **line = '\0'; (*line)++; }
    return start;
}

//=============================================================================
// Debug Board
//=============================================================================

void uci_debug(const Board *b) {
    board_print(b);
    printf("Hash: %016llx\n", (unsigned long long)b->hash);
    printf("EP: %d\n", b->ep_square);
    printf("Castle: %d\n", b->castle);
    printf("In check: %s\n", board_in_check(b) ? "YES" : "NO");

    Movelist ml;
    generate_legal(b, &ml);
    printf("Legal moves: %d\n", ml.count);
    for (int i = 0; i < ml.count && i < 10; i++) {
        char buf[8];
        move_to_uci(&ml.moves[i], buf, false);
        printf("  %s", buf);
    }
    printf("\n");
}

//=============================================================================
// Position
//=============================================================================

void uci_position(Board *b, char *args) {
    char *p = args;

    // Skip "position"
    while (*p == ' ') p++;

    // Handle "startpos" or "fen"
    if (strncmp(p, "startpos", 8) == 0) {
        board_reset(b);
        p += 8;
    } else if (strncmp(p, "fen", 3) == 0) {
        // FEN string: skip past "fen " to the FEN itself
        while (*p && *p != ' ' && *p != '\n') p++;  // skip "fen"
        while (*p == ' ') p++;  // skip space
        char *fen_start = p;
        char *fen_end = fen_start + strlen(fen_start);
        // Find the "moves" token to know where FEN ends
        char *moves_ptr = NULL;
        for (char *q = fen_start; q < fen_end; q++) {
            while (*q == ' ') q++;
            if (strncmp(q, "moves", 5) == 0) { moves_ptr = q; break; }
        }
        if (moves_ptr) {
            char save = *moves_ptr;
            *moves_ptr = '\0';
            board_load_fen(b, fen_start);
            *moves_ptr = save;
            p = moves_ptr + 5;
        } else {
            board_load_fen(b, fen_start);
            p = fen_end;
        }
    } else {
        board_reset(b);
    }

    // Process moves
    while (*p) {
        while (*p == ' ') p++;
        if (!*p || *p == '\n') break;

        char move_str[8] = {0};
        int i = 0;
        while (*p && *p != ' ' && i < 6) move_str[i++] = *p++;

        Move m = move_from_uci(b, move_str);
        if (m.from || m.to) {
            board_do_move(b, &m);
        }
    }
}

//=============================================================================
// Go
//=============================================================================

void uci_go(Board *b, char *args, SearchInfo *info) {
    memset(info, 0, sizeof(SearchInfo));
    info->stop = false;
    stop_flag = false;

    int depth = 0;
    int64_t time_w = 0, time_b = 0;
    int inc_w = 0, inc_b = 0;
    int movestogo = 30;
    int movetime = 0;

    char *p = args;
    while (*p) {
        while (*p == ' ') p++;
        if (!*p) break;

        if (strncmp(p, "depth", 5) == 0) {
            p += 5;
            while (*p == ' ') p++;
            depth = atoi(p);
        } else if (strncmp(p, "movetime", 8) == 0) {
            p += 8;
            while (*p == ' ') p++;
            movetime = atoi(p);
        } else if (strncmp(p, "wtime", 5) == 0) {
            p += 5;
            while (*p == ' ') p++;
            time_w = atoi(p);
        } else if (strncmp(p, "btime", 5) == 0) {
            p += 5;
            while (*p == ' ') p++;
            time_b = atoi(p);
        } else if (strncmp(p, "winc", 4) == 0) {
            p += 4;
            while (*p == ' ') p++;
            inc_w = atoi(p);
        } else if (strncmp(p, "binc", 4) == 0) {
            p += 4;
            while (*p == ' ') p++;
            inc_b = atoi(p);
        } else if (strncmp(p, "movestogo", 9) == 0) {
            p += 9;
            while (*p == ' ') p++;
            movestogo = atoi(p);
        } else if (strncmp(p, "infinite", 8) == 0) {
            p += 8;
            depth = MAX_DEPTH;
        } else if (strncmp(p, "nodes", 5) == 0) {
            p += 5;
            while (*p == ' ') p++;
            // Not implemented
        } else if (strncmp(p, "stop", 4) == 0) {
            search_stop();
            return;
        } else {
            break;
        }
    }

    // Time management
    int64_t time = b->side == WHITE ? time_w : time_b;
    int inc = b->side == WHITE ? inc_w : inc_b;

    if (movetime > 0) {
        info->time_limit = movetime;
    } else if (time > 0) {
        time_init(info, (int)time, inc, movestogo, depth);
    }

    // If depth not set, use iterative deepening with time management
    if (depth == 0) depth = MAX_DEPTH;

    // Run search in background would be better, but for now:
    SearchResult result = search_start(b, info, depth);

    char buf[8];
    move_to_uci(&result.best_move, buf, false);
    printf("bestmove %s\n", buf);
    fflush(stdout);
}

//=============================================================================
// Setoption
//=============================================================================

void uci_setoption(char *args) {
    char *p = args;
    while (*p == ' ') p++;
    if (strncmp(p, "name", 4) == 0) {
        p += 4;
        while (*p == ' ') p++;

        // Parse name value
        char name[64] = {0};
        int i = 0;
        while (*p && *p != ' ' && i < 63) name[i++] = *p++;

        while (*p == ' ') p++;
        if (strncmp(p, "value", 5) == 0) {
            p += 5;
            while (*p == ' ') p++;
        }

        if (strcmp(name, "Hash") == 0) {
            int size = atoi(p);
            tt_init(size);
            printf("info string set Hash to %d MB\n", size);
        } else if (strcmp(name, "ClearHash") == 0) {
            tt_clear();
        }
    }
}

//=============================================================================
// Main UCI Loop
//=============================================================================

void uci_loop(void) {
    printf("id name %s %s\n", ENGINE_NAME, ENGINE_VERSION);
    printf("id author %s\n", ENGINE_AUTHOR);
    printf("option name Hash type spin default 256 min 1 max 65536\n");
    printf("option name ClearHash type button\n");
    printf("uciok\n");
    fflush(stdout);

    Board board;
    board_reset(&board);
    SearchInfo info;

    while (fgets(input_line, sizeof(input_line), stdin)) {
        input_line[strcspn(input_line, "\n")] = 0;

        if (strcmp(input_line, "uci") == 0) {
            printf("id name %s %s\n", ENGINE_NAME, ENGINE_VERSION);
            printf("id author %s\n", ENGINE_AUTHOR);
            printf("uciok\n");
        } else if (strcmp(input_line, "isready") == 0) {
            printf("readyok\n");
        } else if (strcmp(input_line, "ucinewgame") == 0) {
            tt_clear();
            board_reset(&board);
        } else if (strcmp(input_line, "quit") == 0) {
            break;
        } else if (strncmp(input_line, "position", 8) == 0) {
            uci_position(&board, input_line + 9);
        } else if (strncmp(input_line, "go", 2) == 0) {
            uci_go(&board, input_line + 3, &info);
        } else if (strncmp(input_line, "setoption", 9) == 0) {
            uci_setoption(input_line + 10);
        } else if (strcmp(input_line, "d") == 0) {
            uci_debug(&board);
        } else if (strncmp(input_line, "eval", 4) == 0) {
            int score = evaluate(&board);
            printf("eval: %d\n", score);
        } else if (strncmp(input_line, "perft", 5) == 0) {
            int depth = atoi(input_line + 6);
            if (depth <= 0) depth = 5;
            int64_t start = clock();
            uint64_t n = perft(&board, depth);
            int64_t elapsed = (clock() - start) * 1000 / CLOCKS_PER_SEC;
            printf("nodes %llu time %lld nps %lld\n",
                (unsigned long long)n, (long long)elapsed,
                elapsed > 0 ? (long long)(n * 1000 / elapsed) : 0);
        } else if (strncmp(input_line, "divide", 6) == 0) {
            int depth = atoi(input_line + 7);
            if (depth <= 0) depth = 5;
            perft_divide(&board, depth);
        } else if (strncmp(input_line, "bench", 5) == 0) {
            // Benchmark: run perft to depth 6
            printf("Bench: starting...\n");
            int64_t start = clock();
            uint64_t total = 0;
            for (int d = 1; d <= 6; d++) {
                uint64_t n = perft(&board, d);
                total += n;
                printf("  depth %d: %llu\n", d, (unsigned long long)n);
            }
            int64_t elapsed = (clock() - start) * 1000 / CLOCKS_PER_SEC;
            printf("total: %llu in %lld ms\n", (unsigned long long)total, (long long)elapsed);
        } else if (strncmp(input_line, "tune", 4) == 0) {
            // Usage: tune <data_file> [epochs=50] [lr=0.01] [lambda=0.001] [batch=4096]
            char args[256] = {0};
            char filename[256] = {0};
            int epochs = 50;
            double lr = 0.01;
            double lambda = 0.001;
            int batch = 4096;
            if (strlen(input_line) > 5) {
                strncpy(args, input_line + 5, sizeof(args) - 1);
                char *tok = strtok(args, " \t");
                if (tok) { strncpy(filename, tok, sizeof(filename) - 1); tok = strtok(NULL, " \t"); }
                if (tok) epochs = atoi(tok);
                tok = strtok(NULL, " \t"); if (tok) lr = atof(tok);
                tok = strtok(NULL, " \t"); if (tok) lambda = atof(tok);
                tok = strtok(NULL, " \t"); if (tok) batch = atoi(tok);
            }
            if (filename[0] == 0) {
                printf("Usage: tune <data.epd> [epochs] [lr] [lambda] [batch]\n");
                printf("  data.epd: file with FEN \"result\" lines (e.g., from self-play)\n");
                printf("  epochs:   number of SGD iterations (default 50)\n");
                printf("  lr:       learning rate (default 0.01)\n");
                printf("  lambda:   L2 regularization (default 0.001)\n");
                printf("  batch:    mini-batch size (default 4096)\n");
            } else {
                tuner_run(filename, epochs, lr, lambda, batch);
            }
        } else {
            // Unknown command
        }
        fflush(stdout);
    }
}
