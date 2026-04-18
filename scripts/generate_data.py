#!/usr/bin/env python3
"""
Self-Play Data Generator  —  python-chess + robust subprocess I/O
Uses FEN (from python-chess) to set position each time — avoids engine
mis-parsing "position startpos moves ..." and falling out of sync.
"""

import sys, time, subprocess, chess

GAMES  = int(sys.argv[1]) if len(sys.argv) > 1 else 200
DEPTH  = int(sys.argv[2]) if len(sys.argv) > 2 else 4
OUTPUT = sys.argv[3] if len(sys.argv) > 3 else "train.epd"

# ── Engine subprocess ────────────────────────────────────────────────────────
proc = subprocess.Popen(["./LittleGrandmaster"],
                        stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                        text=True, bufsize=1)

def send(cmd):
    proc.stdin.write(cmd + "\n"); proc.stdin.flush()

def read_line(timeout_ms=8000):
    t0 = time.time()
    while (time.time() - t0) * 1000 < timeout_ms:
        line = proc.stdout.readline()
        if line:
            return line.rstrip()
        time.sleep(0.001)
    return None

send("uci")
while (l := read_line()) and "uciok" not in l: pass

send("isready")
while True:
    l = read_line()
    if l is None: break
    if "uciok" in l: continue
    if l == "readyok": break

print("Engine ready", flush=True)

# ── Engine search — set position by FEN (not move list) ─────────────────────
def best_move(fen, depth):
    send(f"position fen {fen}")
    send(f"go depth {depth}")
    while True:
        l = read_line(15000)
        if l is None: return None
        if l.startswith("bestmove"):
            parts = l.split()
            return parts[1] if len(parts) >= 2 else None

# ── Main loop ───────────────────────────────────────────────────────────────
START_FEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"
all_positions = []
start = time.time()
ww = bb = dd = 0

for g in range(1, GAMES + 1):
    board = chess.Board(START_FEN)
    move_list = []
    pos_start = len(all_positions)

    for ply in range(1, 201):
        fen = " ".join(board.fen().split()[:6])

        if ply >= 9:
            all_positions.append((fen, None))

        move_uci = best_move(fen, DEPTH)
        if not move_uci:
            break

        try:
            move = chess.Move.from_uci(move_uci)
        except Exception:
            import random
            legal = list(board.legal_moves)
            if not legal: break
            move = random.choice(legal)
            move_uci = move.uci()

        if move not in board.legal_moves:
            # Engine's board desynced — play random legal move instead
            import random
            legal = list(board.legal_moves)
            if not legal: break
            move = random.choice(legal)
            move_uci = move.uci()

        board.push(move)
        move_list.append(move_uci)

        if board.is_game_over():
            break

    # Determine result
    if board.is_checkmate():
        result = "0-1" if board.turn == chess.WHITE else "1-0"
    elif board.is_stalemate() or board.is_insufficient_material():
        result = "1/2-1/2"
    else:
        result = "1/2-1/2"

    if result == "1-0":   ww += 1
    elif result == "0-1": bb += 1
    else:                 dd += 1

    # Backfill result for positions from this game
    for i in range(pos_start, len(all_positions)):
        all_positions[i] = (all_positions[i][0], result)

    if g % 20 == 0 or g == 1:
        elapsed = time.time() - start
        rate = g / elapsed if elapsed > 0.01 else 0
        print(f"  Game {g:4d}/{GAMES}: [{ww}W {bb}B {dd}D]  "
              f"pos={len(all_positions):5d}  {rate:.1f} g/s", flush=True)

elapsed = time.time() - start
print(f"\n{GAMES} games, {len(all_positions)} positions in {elapsed:.1f}s "
      f"({GAMES/max(elapsed,0.01):.2f} g/s)", flush=True)

with open(OUTPUT, "w") as f:
    for fen, rstr in all_positions:
        f.write(f'{fen} "{rstr}"\n')
print(f"Saved → {OUTPUT}", flush=True)

send("quit")
proc.stdin.close()
proc.wait()
print("Done", flush=True)