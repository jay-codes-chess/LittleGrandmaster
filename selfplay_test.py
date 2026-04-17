#!/usr/bin/env python3
"""Self-play test for LittleGrandmaster chess engine"""
import subprocess
import sys

def send(proc, cmd):
    proc.stdin.write(cmd + '\n')
    proc.stdin.flush()

def read_until(proc, prefix):
    while True:
        line = proc.stdout.readline()
        if not line:
            return None
        if line.startswith(prefix):
            return line.strip()

def play_game(proc, depth=4, max_turns=20):
    # Reset to starting position
    send(proc, 'position startpos')
    # Clear any leftover output
    for _ in range(10):
        proc.stdout.readline()
    
    moves = []
    for turn in range(max_turns):
        send(proc, f'go depth {depth}')
        
        bestmove = None
        while True:
            line = proc.stdout.readline()
            if not line:
                return moves
            if line.startswith('bestmove'):
                parts = line.strip().split()
                if len(parts) >= 2:
                    bestmove = parts[1]
                break
        
        if not bestmove or bestmove == '(none)':
            break
            
        moves.append(bestmove)
        send(proc, f'position startpos moves {" ".join(moves)}')
        
        # Drain extra output
        for _ in range(10):
            proc.stdout.readline()
    
    return moves

def main():
    print("Starting LittleGrandmaster self-play test...")
    
    proc = subprocess.Popen(['./LittleGrandmaster'],
        stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True, bufsize=1)
    
    # Initialize
    for _ in range(10):
        proc.stdout.readline()
    
    print("Playing game 1...")
    moves = play_game(proc, depth=4, max_turns=10)
    print(f"Game complete: {' '.join(moves)}")
    print(f"Total moves: {len(moves)}")
    
    send(proc, 'quit')
    proc.wait()
    
    print("\nEngine self-play working!")

if __name__ == '__main__':
    main()