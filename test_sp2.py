#!/usr/bin/env python3
import subprocess
import re

ENGINE = './LittleGrandmaster'

def send(proc, cmd):
    proc.stdin.write(cmd + '\n')
    proc.stdin.flush()

def read_line(proc):
    return proc.stdout.readline()

def get_bestmove(proc, depth=6):
    send(proc, f'go depth {depth}')
    while True:
        line = read_line(proc)
        if not line:
            return None
        line = line.strip()
        if line.startswith('bestmove'):
            parts = line.split()
            return parts[1] if len(parts) > 1 else None
        # Skip other output

def main():
    proc = subprocess.Popen([ENGINE],
        stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
        text=True, bufsize=1)

    # Discard startup lines
    for _ in range(6):
        read_line(proc)

    moves = []
    for turn in range(1, 7):
        if turn == 1:
            send(proc, 'position startpos')
        else:
            send(proc, f'position startpos moves {" ".join(moves)}')

        move = get_bestmove(proc, depth=5)
        if not move:
            print(f"No move at turn {turn}")
            break
        print(f"Turn {turn}: {move}")
        moves.append(move)

    send(proc, 'quit')
    proc.wait()

    print(f"\nGame: {' '.join(moves)}")

if __name__ == '__main__':
    main()