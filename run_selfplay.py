import subprocess, sys, time

ENGINE = "/home/jay/LittleGrandmaster/LittleGrandmaster"

class Engine:
    def __init__(self, name, depth):
        self.name = name
        self.depth = depth
        self.proc = None

    def start(self):
        self.proc = subprocess.Popen(
            [ENGINE], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
            stderr=subprocess.PIPE, cwd="/home/jay/LittleGrandmaster", text=True, bufsize=1)
        self._send("uci")
        self._read(lambda l: "uciok" in l)
        self._send("ucinewgame")
        self._send("isready")
        self._read(lambda l: "readyok" in l)

    def _send(self, cmd):
        self.proc.stdin.write(cmd + "\n")
        self.proc.stdin.flush()

    def _read(self, pred, timeout=30):
        start = time.time()
        while time.time() - start < timeout:
            line = self.proc.stdout.readline()
            if not line:
                return None
            if pred(line):
                return line
        return None

    def setpos(self, moves):
        if moves:
            self._send(f"position startpos moves {' '.join(moves)}")
        else:
            self._send("position startpos")

    def go(self):
        self._send(f"go depth {self.depth}")
        line = self._read(lambda l: "bestmove" in l, timeout=60)
        if not line:
            return None
        parts = line.strip().split()
        move = parts[1] if len(parts) >= 2 else None
        if move and len(parts) >= 3 and parts[2] != "(none)":
            move += parts[2]
        return move

    def quit(self):
        if self.proc:
            self._send("quit")
            try:
                self.proc.wait(timeout=3)
            except:
                self.proc.kill()

def play_game(d1, d2, verbose=False):
    w = Engine(f"LGM-d{d1}", d1)
    b = Engine(f"LGM-d{d2}", d2)
    w.start()
    b.start()

    moves = []
    try:
        for ply in range(300):
            eng = w if len(moves) % 2 == 0 else b
            eng.setpos(moves if moves else None)
            move = eng.go()
            if not move or move == "(none)":
                break
            moves.append(move)
            if verbose and len(moves) <= 30:
                print(f"  {len(moves)}. {move}")

            other = b if eng is w else w
            other.setpos(moves if moves else None)
    finally:
        w.quit()
        b.quit()

    return moves, "*"

def to_pgn(moves, wname, bname, result):
    now = "2026.04.18"
    pgn = f'''[Event "Test"]
[Site "Local"]
[Date "{now}"]
[Round "1"]
[White "{wname}"]
[Black "{bname}"]
[Result "{result}"]

'''
    for i in range(0, len(moves), 2):
        mn = i // 2 + 1
        wm = moves[i]
        bm = moves[i + 1] if i + 1 < len(moves) else ""
        if bm:
            pgn += f"{mn}. {wm} {bm}\n"
        else:
            pgn += f"{mn}. {wm}\n"
    pgn += f"{result}\n"
    return pgn

if __name__ == "__main__":
    print("Playing LGM-d3 vs LGM-d4...")
    moves, result = play_game(3, 4, verbose=True)
    pgn = to_pgn(moves, "LGM-d3", "LGM-d4", result)
    print(f"Game: {len(moves)} moves, {result}")
    print(pgn)