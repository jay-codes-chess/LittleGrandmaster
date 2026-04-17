#!/usr/bin/env python3
import subprocess
import time
import sys

def send_cmd(proc, cmd):
    proc.stdin.write(cmd + '\n')
    proc.stdin.flush()

def read_until(proc, marker, timeout=10):
    start = time.time()
    lines = []
    while time.time() - start < timeout:
        line = proc.stdout.readline()
        if not line:
            break
        lines.append(line.rstrip())
        if marker in line:
            break
    return lines

def main():
    proc = subprocess.Popen(['./LittleGrandmaster'],
        stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, 
        text=True, bufsize=1)
    
    for _ in range(10):
        proc.stdout.readline()
    
    send_cmd(proc, 'position startpos')
    send_cmd(proc, 'go depth 8')
    
    lines = read_until(proc, 'bestmove', timeout=20)
    for l in lines:
        print(l)
    
    proc.stdin.write('quit\n')
    proc.stdin.flush()
    proc.wait()

if __name__ == '__main__':
    main()