#!/usr/bin/env python3
"""
Simple self-play harness for LittleGrandmaster.
Uses engine's position tracking to avoid manual FEN parsing bugs.
"""

import subprocess
import sys
import os
import time
import random
from datetime import datetime

ENGINE_CMD = "./LittleGrandmaster"
WORKING_DIR = "/home/jay/LittleGrandmaster"

def create_engine(name, depth=4):
    """Create a UCI engine instance."""
    proc = subprocess.Popen(
        [ENGINE_CMD],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        cwd=WORKING_DIR,
        text=True,
        bufsize=1
    )
    
    # UCI handshake
    def send(cmd):
        proc.stdin.write(cmd + "\n")
        proc.stdin.flush()
    
    def read_until(predicate, timeout=30):
        """Read lines until predicate returns True."""
        start = time.time()
        while time.time() - start < timeout:
            line = proc.stdout.readline()
            if not line:
                return None
            if predicate(line):
                return line
        return None
    
    send("uci")
    read_until(lambda l: "uciok" in l)
    send("isready")
    read_until(lambda l: "readyok" in l)
    
    return {
        "proc": proc,
        "name": name,
        "depth": depth,
        "send": send,
        "read_until": read_until,
        "history": []
    }

def close_engine(eng):
    eng["send"]("quit")
    try:
        eng["proc"].wait(timeout=3)
    except subprocess.TimeoutExpired:
        eng["proc"].kill()

def ucinewgame(eng):
    eng["send"]("ucinewgame")
    eng["send"]("isready")
    eng["read_until"](lambda l: "readyok" in l)

def setoption(eng, name, value):
    eng["send"](f"setoption name {name} value {value}")

def position(eng, moves=None, fen=None):
    if fen:
        cmd = f"position fen {fen}"
    else:
        cmd = "position startpos"
        if moves:
            cmd += " moves " + " ".join(moves)
    eng["send"](cmd)

def go(eng, depth=None):
    if depth:
        cmd = f"go depth {depth}"
    else:
        cmd = "go depth 10"
    eng["send"](cmd)
    
    # Read until bestmove
    while True:
        line = eng["read_until"](lambda l: "bestmove" in l, timeout=60)
        if not line:
            return None, []
        
        parts = line.strip().split()
        if len(parts) < 2:
            return None, []
        
        move = parts[1]
        # Handle promotion
        if len(parts) >= 3 and parts[2] != "(none)":
            move = parts[1] + parts[2]
        
        # Get PV line for info
        pv = []
        for i, p in enumerate(parts):
            if p == "pv":
                pv = parts[i+1:]
                break
        
        return move, pv

def play_game(engine_white_cfg, engine_black_cfg, max_moves=200, verbose=False):
    """Play a game between two engine configurations."""
    
    # Create engines
    white = create_engine(engine_white_cfg["name"], engine_white_cfg["depth"])
    black = create_engine(engine_black_cfg["name"], engine_black_cfg["depth"])
    
    try:
        # Set options
        setoption(white, "Hash", 128)
        setoption(black, "Hash", 128)
        
        # New game
        ucinewgame(white)
        ucinewgame(black)
        
        # Reset engines to start position
        position(white)
        position(black)
        
        moves = []
        
        for ply in range(max_moves):
            # Determine which engine to query
            if len(moves) % 2 == 0:
                eng = white
            else:
                eng = black
            
            # Set position on active engine (moves so far)
            position(eng, moves=moves if moves else None)
            
            # Make move
            move, pv = go(eng, depth=eng["depth"])
            
            if not move or move == "(none)":
                # Stalemate or game end
                break
            
            moves.append(move)
            
            if verbose and len(moves) <= 20:
                print(f"  {len(moves)}. {move}", file=sys.stderr)
            
            # Sync to other engine by setting same position
            # (Simpler than applying moves)
            if len(moves) % 2 == 0:
                # White just moved, sync black
                position(black, moves=moves)
            else:
                # Black just moved, sync white
                position(white, moves=moves)
            
            # Check for repetition or 50-move rule (rough)
            if len(moves) > 100 and len(set(moves[-50:])) < 5:
                break
        
        # Determine result (simplified - could add proper game state detection)
        result = "*"
        
        return moves, result
    
    finally:
        close_engine(white)
        close_engine(black)

def moves_to_pgn(moves, white_name, black_name, result="*"):
    """Convert move list to PGN."""
    now = datetime.now().strftime("%Y.%m.%d")
    
    pgn = f"""[Event "Engine Test"]
[Site "Local"]
[Date "{now}"]
[Round "1"]
[White "{white_name}"]
[Black "{black_name}"]
[Result "{result}"]

"""
    
    for i in range(0, len(moves), 2):
        move_num = i // 2 + 1
        white_move = moves[i]
        black_move = moves[i+1] if i+1 < len(moves) else ""
        
        if black_move:
            pgn += f"{move_num}. {white_move} {black_move}"
        else:
            pgn += f"{move_num}. {white_move}"
        
        if i + 2 < len(moves):
            pgn += "\n"
    
    pgn += f"\n{result}\n"
    return pgn

def main():
    import argparse
    parser = argparse.ArgumentParser(description="Engine self-play harness")
    parser.add_argument("-d1", "--depth1", type=int, default=3)
    parser.add_argument("-d2", "--depth2", type=int, default=4)
    parser.add_argument("-n", "--games", type=int, default=1)
    parser.add_argument("-o", "--output", type=str, default=None)
    parser.add_argument("-v", "--verbose", action="store_true")
    args = parser.parse_args()
    
    print(f"Playing {args.games} game(s): LGM-d{args.depth1} (white) vs LGM-d{args.depth2} (black)")
    
    all_pgn = []
    for i in range(args.games):
        print(f"\n--- Game {i+1}/{args.games} ---", file=sys.stderr)
        
        w_cfg = {"name": f"LGM-d{args.depth1}", "depth": args.depth1}
        b_cfg = {"name": f"LGM-d{args.depth2}", "depth": args.depth2}
        
        moves, result = play_game(w_cfg, b_cfg, max_moves=300, verbose=args.verbose)
        
        pgn = moves_to_pgn(moves, w_cfg["name"], b_cfg["name"], result)
        all_pgn.append(pgn)
        
        print(f"Game ended after {len(moves)} moves: {result}")
        if args.verbose:
            print(pgn)
    
    if args.output:
        with open(args.output, "w") as f:
            f.write("\n".join(all_pgn))
        print(f"PGN saved to {args.output}", file=sys.stderr)
    
    return all_pgn

if __name__ == "__main__":
    main()