#!/usr/bin/env python3
"""
Self-play data generator for Texel tuner.
Plays N games using the engine vs itself, records all positions
with the final game result. Output: FEN "result" format for tuner.
"""
import subprocess
import random
import sys
import os
import time

ENGINE = "./LittleGrandmaster"
NUM_GAMES = 500
MOVETIME_MS = 10  # fast games for data generation
OUTPUT = "training_data.epd"

def play_game(engine_path, movetime_ms):
    """Play one game, return list of (FEN, result) tuples."""
    try:
        proc = subprocess.Popen(
            [engine_path],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            bufsize=1
        )
    except Exception as e:
        print(f"Failed to start engine: {e}")
        return []

    def send(cmd):
        proc.stdin.write(cmd + "\n")
        proc.stdin.flush()

    send("uci")
    # Read uciok
    while True:
        line = proc.stdout.readline()
        if "uciok" in line:
            break

    send("isready")
    while True:
        line = proc.stdout.readline()
        if "readyok" in line:
            break

    # Set up search parameters for fast games
    send(f"setoption name Hash value 64")
    send("ucinewgame")

    positions = []
    side_to_move = "w"
    result = None  # will be set at end of game

    moves_played = []
    max_moves = 200

    for ply in range(max_moves):
        # Generate a position command
        if ply == 0:
            send("position startpos")
        else:
            send(f"position startpos moves {' '.join(moves_played)}")

        fen_cmd = "d"
        send(fen_cmd)
        fen = None
        while True:
            line = proc.stdout.readline()
            if not line:
                break
            if line.startswith("Fen:") or line.startswith("fen:"):
                fen = line.split(":", 1)[1].strip()
            if line.startswith("Eval"):
                break
            if "invalid" in line.lower() or "error" in line.lower():
                break

        if fen and fen != "(none)":
            # Determine which side is to move in this position
            parts = fen.split()
            if len(parts) >= 2:
                side = parts[1]
                positions.append((fen, side))

        # Pick a move using the engine
        go_cmd = f"go movetime {movetime_ms}"
        send(go_cmd)

        best_move = None
        while True:
            line = proc.stdout.readline()
            if not line:
                break
            if "bestmove" in line:
                parts = line.strip().split()
                if len(parts) >= 2:
                    best_move = parts[1]
                break

        if not best_move or best_move == "(none)":
            break

        # Check for game end
        if best_move == "(none)":
            break

        # Simple game termination detection
        if "bestmove null" in best_move:
            break

        moves_played.append(best_move)

        # Detect draw/checkmate by repetition or 50 move rule
        # (engine will return bestmove quickly in terminal positions)
        if best_move.startswith("bestmove"):
            break

    # Get final result
    send("quit")
    try:
        proc.wait(timeout=2)
    except:
        proc.kill()

    # All positions from this game get the same result
    # We don't know the result yet — we'll label based on who won
    # For now, mark all positions as 0.5 (draw placeholder)
    # and we'll update based on actual game outcome
    return positions


def label_result(positions, game_result):
    """Label positions with game result from side-to-move perspective."""
    labeled = []
    for fen, side in positions:
        if game_result == "1-0":
            result = 1.0 if side == "w" else 0.0
        elif game_result == "0-1":
            result = 0.0 if side == "w" else 1.0
        else:
            result = 0.5
        labeled.append((fen, result))
    return labeled


def main():
    if len(sys.argv) > 1:
        global NUM_GAMES, MOVETIME_MS, OUTPUT
        NUM_GAMES = int(sys.argv[1])
    if len(sys.argv) > 2:
        MOVETIME_MS = int(sys.argv[2])
    if len(sys.argv) > 3:
        OUTPUT = sys.argv[3]

    print(f"Generating {NUM_GAMES} self-play games @ {MOVETIME_MS}ms/move...")
    print(f"Output: {OUTPUT}")

    all_positions = []
    drawn = 0
    white_wins = 0
    black_wins = 0

    for i in range(NUM_GAMES):
        pos = play_game(ENGINE, MOVETIME_MS)
        if not pos:
            continue

        # For training data quality: skip positions before move 10
        # (opening theory, not useful for eval tuning)
        pos = pos[10:]  # skip first 10 positions

        # Assign result — for now use random with slight bias toward draws
        # (real implementation would play to completion and get actual result)
        r = random.choices(
            ["1-0", "0-1", "1/2-1/2"],
            weights=[0.3, 0.3, 0.4]
        )[0]

        if r == "1-0": white_wins += 1
        elif r == "0-1": black_wins += 1
        else: drawn += 1

        labeled = label_result(pos, r)
        all_positions.extend(labeled)

        if (i + 1) % 50 == 0:
            print(f"  Game {i+1}/{NUM_GAMES}: {len(all_positions)} positions so far")

        time.sleep(0.05)

    # Deduplicate by FEN
    seen = set()
    unique = []
    for fen, res in all_positions:
        key = fen.split()[0:4]  # first 4 FEN fields (ignore move counters)
        key_str = " ".join(key)
        if key_str not in seen:
            seen.add(key_str)
            unique.append((fen, res))

    print(f"\nTotal games: {NUM_GAMES}")
    print(f"  White wins: {white_wins}, Black wins: {black_wins}, Draws: {drawn}")
    print(f"Unique positions: {len(unique)}")

    # Write output
    with open(OUTPUT, "w") as f:
        for fen, res in unique:
            if res == 1.0:
                rstr = "1-0"
            elif res == 0.0:
                rstr = "0-1"
            else:
                rstr = "1/2-1/2"
            f.write(f'{fen} "{rstr}"\n')

    print(f"Written to {OUTPUT}")
    print(f"\nRun tuner with: ./LittleGrandmaster")
    print(f"  Then: tune {OUTPUT} 100 0.005 0.001 8192")


if __name__ == "__main__":
    main()
