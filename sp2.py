#!/usr/bin/env python3
import subprocess, sys, time

def main():
    ENG = "/home/jay/LittleGrandmaster/LittleGrandmaster"
    
    class E:
        def __init__(self, n, d):
            self.n = n; self.d = d; self.p = None
        def start(self):
            self.p = subprocess.Popen([ENG], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, cwd="/home/jay/LittleGrandmaster", text=True, bufsize=1)
            self.send("uci"); self.read(lambda l: "uciok" in l)
            self.send("ucinewgame"); self.send("isready"); self.read(lambda l: "readyok" in l)
        def send(self, c): self.p.stdin.write(c+"\n"); self.p.stdin.flush()
        def read(self, p, t=30):
            s = time.time()
            while time.time()-s < t:
                l = self.p.stdout.readline()
                if not l: return None
                if p(l): return l
            return None
        def setpos(self, m):
            cmd = "position startpos" if not m else f"position startpos moves {' '.join(m)}"
            self.send(cmd)
        def go(self):
            self.send(f"go depth {self.d}")
            l = self.read(lambda x: "bestmove" in x, 60)
            if not l: return None
            parts = l.strip().split()
            return parts[1] if len(parts) >= 2 else None
        def quit(self):
            if self.p:
                self.send("quit")
                try: self.p.wait(3)
                except: self.p.kill()

    def play(d1, d2, verbose=False):
        w, b = E(f"LGM-d{d1}", d1), E(f"LGM-d{d2}", d2)
        w.start(); b.start()
        moves = []
        try:
            for _ in range(200):
                e = w if len(moves) % 2 == 0 else b
                e.setpos(moves if moves else None)
                mv = e.go()
                if not mv: break
                moves.append(mv)
                if verbose and len(moves) <= 20:
                    print(f"  {len(moves)}. {mv}")
                o = b if e is w else w
                o.setpos(moves if moves else None)
        finally:
            w.quit(); b.quit()
        return moves

    print("Playing LGM-d3 vs LGM-d4...")
    moves = play(3, 4, verbose=True)
    print(f"\nGame ended after {len(moves)} moves")

    # Output PGN
    pgn = '[Event "Test"]\n[Site "Local"]\n[Date "2026.04.18"]\n[Round "1"]\n[White "LGM-d3"]\n[Black "LGM-d4"]\n[Result "*"]\n\n'
    for i in range(0, len(moves), 2):
        n = i//2+1; wm = moves[i]; bm = moves[i+1] if i+1 < len(moves) else ""
        pgn += f"{n}. {wm}" + (f" {bm}\n" if bm else "\n")
    pgn += "*\n"
    print(pgn)

main()