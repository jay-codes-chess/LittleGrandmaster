#ifndef UCI_H
#define UCI_H

#include "types.h"
#include "board.h"

//=============================================================================
// UCI Protocol
//=============================================================================

void uci_loop(void);

void uci_position(Board *b, char *args);
void uci_go(Board *b, char *args, SearchInfo *info);
void uci_setoption(char *args);

// Send to stdout
void uci_send(const char *fmt, ...);
void uci_send_bestmove(Move *m, Move *ponder);
void uci_send_info(int depth, int seldepth, int64_t nodes, int64_t time_ms, int nps, int hashfull, int pv_len, Move *pv, int score, int score_type);
void uci_send_info_str(const char *str);

// Parse UCI move
Move uci_parse_move(const Board *b, const char *str);

// Engine info
#define ENGINE_NAME    "Little Grandmaster"
#define ENGINE_AUTHOR  "Brendan & Jay"
#define ENGINE_VERSION "0.1"
#define ENGINE_MMHH    "2026"

// Debug
void uci_debug(const Board *b);

#endif // UCI_H
