#!/usr/bin/env python3
import subprocess

proc = subprocess.Popen(['./LittleGrandmaster'],
    stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True)

proc.stdin.write('position startpos\n')
proc.stdin.write('go depth 4\n')
proc.stdin.flush()

for _ in range(20):
    line = proc.stdout.readline()
    if not line:
        break
    print(line.rstrip())
    if line.startswith('bestmove'):
        break

proc.stdin.write('quit\n')
proc.stdin.flush()
proc.wait()