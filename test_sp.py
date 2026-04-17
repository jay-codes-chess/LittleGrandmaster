#!/usr/bin/env python3
import subprocess

proc = subprocess.Popen(['./LittleGrandmaster'],
    stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)

# Position
proc.stdin.write('position startpos\n')
proc.stdin.flush()
for _ in range(6):
    proc.stdout.readline()

# Turn 1
proc.stdin.write('go depth 5\n')
proc.stdin.flush()
bestmove = None
while True:
    line = proc.stdout.readline()
    if not line:
        break
    print(line.rstrip())
    if line.startswith('bestmove'):
        bestmove = line.split()[1]
        break

if bestmove:
    print(f"\nBest move: {bestmove}")
    moves1 = bestmove
    # Turn 2
    proc.stdin.write(f'position startpos moves {moves1}\n')
    proc.stdin.flush()
    for _ in range(6):
        proc.stdout.readline()
    proc.stdin.write('go depth 5\n')
    proc.stdin.flush()
    while True:
        line = proc.stdout.readline()
        if not line:
            break
        print(line.rstrip())
        if line.startswith('bestmove'):
            break

proc.stdin.write('quit\n')
proc.stdin.flush()
proc.wait()