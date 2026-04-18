#!/usr/bin/env python3
"""
PeSTO Alignment Tuner v4 — Normalized Features + L2 Regularization
===================================================================
For each position:
  target  = PeSTO eval (material + PST, no mobility)
  actual  = Engine eval (current params baked in)

Tuning goal: find params such that:
  Engine_eval ≈ PeSTO_eval + mobility_contribution

Where mobility_contribution = Σ param_i × normalized_feature_i

Normalized features divide by baseline attack count so they're ~0.1 per extra attack.
LAMBDA prevents params from growing unbounded.

Key engine formula (from mobility_score):
  mobN_contrib = MOBILITY_KNIGHT * (attacks - 4)
  mobB_contrib = MOBILITY_BISHOP * (attacks - 7)
  mobR_contrib = MOBILITY_ROOK   * (attacks - 7)
  mobQ_contrib = MOBILITY_QUEEN  * (attacks - 14)
  rook_contrib = ROOK_OPEN_FILE * open_files + ROOK_SEMIOPEN * semiopen_files
  bishop_pair  = BISHOP_PAIR_SCORE * has_pair
"""

import sys, math, time, subprocess, chess

EPOCHS  = int(sys.argv[2]) if len(sys.argv) > 2 else 500
LR      = float(sys.argv[3]) if len(sys.argv) > 3 else 1.0
LAMBDA  = float(sys.argv[4]) if len(sys.argv) > 4 else 0.01
DATA    = sys.argv[1]
ENGINE  = "./LittleGrandmaster"

print(f"Tuner: data={DATA}, epochs={EPOCHS}, lr={LR}, lambda={LAMBDA}", flush=True)

# ── Load positions ─────────────────────────────────────────────────────────
positions = []
with open(DATA) as f:
    for line in f:
        line = line.strip()
        if not line or '"' not in line: continue
        fen, rest = line.split('"', 1)
        r = rest.split('"')[0].strip()
        result = {"1-0": 1.0, "0-1": 0.0, "1/2-1/2": 0.5}.get(r, 0.5)
        positions.append((fen.strip(), result))

print(f"Loaded {len(positions)} positions", flush=True)

# ── Engine subprocess ─────────────────────────────────────────────────────────
proc = subprocess.Popen([ENGINE], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                        text=True, bufsize=1)
def send(cmd): proc.stdin.write(cmd + "\n"); proc.stdin.flush()
def read_line(ms=5000):
    t0 = time.time()
    while (time.time() - t0) * 1000 < ms:
        l = proc.stdout.readline()
        if l: return l.rstrip()
        time.sleep(0.001)
    return None

send("uci")
while (l := read_line()) and "uciok" not in l: pass
send("isready")
while True:
    l = read_line()
    if l is None: break
    if l == "readyok": break
    if "uciok" in l: continue

print("Engine ready", flush=True)

# ── PeSTO tables ─────────────────────────────────────────────────────────────
MG_VALUE = {chess.PAWN: 100, chess.KNIGHT: 380, chess.BISHOP: 418,
            chess.ROOK: 495, chess.QUEEN: 1025}
PST = {
    chess.PAWN: [[0,0,0,0,0,0,0,0],[78,83,86,73,83,82,85,86],
                 [7,29,21,44,31,21,31,7],[-17,16,8,28,17,16,3,-17],
                 [-26,9,3,-1,3,3,0,-26],[-22,9,3,-7,3,3,0,-22],
                 [-26,9,3,-7,3,3,0,-26],[0,0,0,0,0,0,0,0]],
    chess.KNIGHT: [[-66,-75,-65,-52,-72,-55,-58,-76],[-50,-34,-18,-13,-12,-6,-8,-26],
                   [-21,-12,5,0,4,3,14,-5],[-5,1,9,14,10,6,5,-4],
                   [0,2,7,11,8,4,2,-1],[-5,1,9,11,8,4,2,-4],
                   [-12,-2,5,0,4,2,-2,-12],[-63,-65,-42,-38,-52,-38,-43,-76]],
    chess.BISHOP: [[-33,-21,-16,-10,-6,-4,-12,-22],[-21,-12,-10,4,3,3,-3,-7],
                   [-17,-9,-3,1,1,0,-2,-9],[-6,-1,1,5,6,5,2,-4],
                   [-1,3,6,8,7,8,4,-2],[-5,2,5,9,8,8,3,-5],
                   [-11,-11,-10,3,3,2,-3,-10],[-29,-21,-24,-14,-17,-13,-21,-30]],
    chess.ROOK: [[32,0,0,0,0,0,0,32],[27,0,0,0,0,0,0,27],
                [23,0,0,0,0,0,0,23],[17,0,0,0,0,0,0,17],
                [9,0,0,0,0,0,0,9],[-7,0,0,0,0,0,0,-7],
                [-21,0,0,0,0,0,0,-21],[-10,0,0,0,0,0,0,-10]],
    chess.QUEEN: [[0,4,6,12,8,6,4,0],[4,0,2,6,4,2,0,4],
                 [6,2,0,4,4,0,2,6],[12,6,4,0,2,4,6,12],
                 [8,4,4,2,0,2,4,8],[6,2,0,4,2,0,2,6],
                 [4,0,2,6,4,2,0,4],[0,4,6,12,8,6,4,0]],
}

def pesto_eval(fen):
    board = chess.Board(fen)
    score = 0.0
    for sq in chess.SQUARES:
        pc = board.piece_at(sq)
        if pc is None or pc.piece_type == chess.KING: continue
        r = chess.square_rank(sq)
        c = chess.square_file(sq)
        if pc.color == chess.BLACK: r = 7 - r
        val = MG_VALUE[pc.piece_type] + PST[pc.piece_type][r][c]
        if pc.color == chess.WHITE: score += val
        else: score -= val
    return score * 2.0

def extract_features(fen):
    """Extract engine-compatible features with normalized counts."""
    board = chess.Board(fen)

    # Raw attack counts
    mn = mb = mr = mq = 0
    for mv in board.legal_moves:
        pt = board.piece_type_at(mv.from_square)
        if pt == chess.KNIGHT: mn += 1
        elif pt == chess.BISHOP: mb += 1
        elif pt == chess.ROOK: mr += 1
        elif pt == chess.QUEEN: mq += 1

    # Bishop pair
    w_bp = 1 if len(board.pieces(chess.BISHOP, chess.WHITE)) >= 2 else 0
    b_bp = 1 if len(board.pieces(chess.BISHOP, chess.BLACK)) >= 2 else 0

    # Rook files
    w_rooks = list(board.pieces(chess.ROOK, chess.WHITE))
    b_rooks = list(board.pieces(chess.ROOK, chess.BLACK))
    pawn_files = {chess.square_file(p) for p in
                  set(board.pieces(chess.PAWN, chess.WHITE)) |
                  set(board.pieces(chess.PAWN, chess.BLACK))}

    w_ro = w_rs = b_ro = b_rs = 0
    for r in w_rooks:
        f = chess.square_file(r)
        if f not in pawn_files: w_ro += 1
        else: w_rs += 1
    for r in b_rooks:
        f = chess.square_file(r)
        if f not in pawn_files: b_ro += 1
        else: b_rs += 1

    # Raw features: attack_count - baseline (EXACTLY matches engine mobility formula)
    # Engine: mobN_contrib = MOBILITY_KNIGHT * (attacks_N - 4)
    # So feature = (attacks - baseline), param corresponds 1:1 with cp
    return {
        "mobN": mn - 4,    # 0 = baseline 4 attacks, ±1 per extra/missing attack
        "mobB": mb - 7,    # 0 = baseline 7 attacks
        "mobR": mr - 7,    # 0 = baseline 7 attacks
        "mobQ": mq - 14,   # 0 = baseline 14 attacks
        "bishop_pair": w_bp - b_bp,  # 1 = has pair, 0 = no pair, -1 = opponent has pair
        "rook_open": w_ro - b_ro,    # count difference
        "rook_semiopen": w_rs - b_rs,  # count difference
    }

def get_eval(fen):
    send(f"position fen {fen}")
    send("eval")
    for _ in range(20):
        l = read_line(3000)
        if l is None: break
        if "eval:" in l.lower():
            return float(l.split(":")[1].strip())
    return None

# ── Collect all data ─────────────────────────────────────────────────────────
print("Collecting data...", flush=True)
t0 = time.time()
engine_evals = []
pesto_evals  = []
features     = []

for i, (fen, result) in enumerate(positions):
    ev = get_eval(fen)
    if ev is None: continue
    pest = pesto_eval(fen)
    feat = extract_features(fen)
    engine_evals.append(ev)
    pesto_evals.append(pest)
    features.append(feat)
    if (i+1) % 300 == 0:
        print(f"  {i+1}/{len(positions)}", flush=True)

print(f"Got {len(engine_evals)} positions in {time.time()-t0:.1f}s", flush=True)
if len(engine_evals) == 0:
    print("FATAL"); send("quit"); proc.stdin.close(); sys.exit(1)

# ── Params ──────────────────────────────────────────────────────────────────
class P:
    def __init__(self):
        # Tunable params (baseline from Stockfish/PeSTO)
        self.mobN = 4.0;   self.mobB = 5.0;   self.mobR = 3.0;   self.mobQ = 2.0
        self.rook_open = 12.0; self.rook_semiopen = 6.0
        self.bishop_pair = 30.0; self.tempo = 10.0

    def as_dict(self):
        return {"mobN": self.mobN, "mobB": self.mobB, "mobR": self.mobR, "mobQ": self.mobQ,
                "rook_open": self.rook_open, "rook_semiopen": self.rook_semiopen,
                "bishop_pair": self.bishop_pair, "tempo": self.tempo}

    def l2(self): return sum(v*v for v in self.as_dict().values())

    def apply(self, g, lr):
        self.mobN  -= lr * 0.01 * g.get("mobN",  0)
        self.mobB  -= lr * 0.01 * g.get("mobB",  0)
        self.mobR  -= lr * 0.01 * g.get("mobR",  0)
        self.mobQ  -= lr * 0.01 * g.get("mobQ",  0)
        self.rook_open    -= lr * 0.1  * g.get("rook_open",    0)
        self.rook_semiopen -= lr * 0.1  * g.get("rook_semiopen", 0)
        self.bishop_pair   -= lr * 0.1  * g.get("bishop_pair",  0)
        self.tempo         -= lr * 0.1  * g.get("tempo",        0)

BASELINE = {"mobN": 4, "mobB": 5, "mobR": 3, "mobQ": 2,
            "rook_open": 12, "rook_semiopen": 6, "bishop_pair": 30, "tempo": 10}

def model_eval(i, p):
    """Engine eval ≈ pesto_eval + param_correction.
    Param correction = Σ (param_i - baseline_i) * feature_i."""
    f = features[i]
    delta = 0.0
    delta += (p.mobN  - BASELINE["mobN"])  * f["mobN"]
    delta += (p.mobB  - BASELINE["mobB"])  * f["mobB"]
    delta += (p.mobR  - BASELINE["mobR"])  * f["mobR"]
    delta += (p.mobQ  - BASELINE["mobQ"])  * f["mobQ"]
    delta += (p.bishop_pair  - BASELINE["bishop_pair"])  * f["bishop_pair"]
    delta += (p.rook_open    - BASELINE["rook_open"])    * f["rook_open"]
    delta += (p.rook_semiopen- BASELINE["rook_semiopen"]) * f["rook_semiopen"]
    return engine_evals[i] + delta

def sq_error(p):
    total = 0.0
    for i in range(len(engine_evals)):
        d = model_eval(i, p) - pesto_evals[i]
        total += d * d
    total += LAMBDA * p.l2()
    return total / len(engine_evals)

# ── Verify gradient ──────────────────────────────────────────────────────────
params = P()
initial = sq_error(params)
p_test = P()
p_test.mobN = 6.0
print(f"\nInitial sq-error: {initial:.2f}  mobN=6: {sq_error(p_test):.2f}", flush=True)

# ── SGD ─────────────────────────────────────────────────────────────────────
best_err  = initial
best_state = {k: getattr(params, k) for k in params.as_dict()}

print(f"\n{'Epoch':>5}  {'SqErr':>12}  |  mobN  mobB  mobR  mobQ  |  ro   rs   bp   tempo",
      flush=True)
print("-" * 80, flush=True)

for epoch in range(1, EPOCHS + 1):
    grad = {}
    delta = 0.05  # small perturbation for numeric gradient

    for name in params.as_dict():
        orig = getattr(params, name)
        setattr(params, name, orig + delta); ep = sq_error(params)
        setattr(params, name, orig - delta); em = sq_error(params)
        grad[name] = (ep - em) / (2 * delta)
        setattr(params, name, orig)

    params.apply(grad, LR)
    err = sq_error(params)

    if err < best_err:
        best_err = err
        best_state = {k: getattr(params, k) for k in params.as_dict()}

    if epoch % 50 == 0 or epoch == 1:
        print(f"{epoch:5d}  {err:12.4f}  | "
              f"{params.mobN:5.2f} {params.mobB:5.2f} {params.mobR:5.2f} {params.mobQ:5.2f}  | "
              f"{params.rook_open:5.1f} {params.rook_semiopen:5.1f} "
              f"{params.bishop_pair:5.1f} {params.tempo:5.1f}",
              flush=True)

for k, v in best_state.items():
    setattr(params, k, v)

print(f"\nFinal sq-err: {best_err:.4f} (was {initial:.4f})  "
      f"improvement={(1-best_err/max(initial,0.1))*100:.2f}%", flush=True)

print("\n// Paste into include/eval.h:")
print(f"#define MOBILITY_KNIGHT   {round(params.mobN)}")
print(f"#define MOBILITY_BISHOP   {round(params.mobB)}")
print(f"#define MOBILITY_ROOK     {round(params.mobR)}")
print(f"#define MOBILITY_QUEEN    {round(params.mobQ)}")
print(f"#define ROOK_OPEN_FILE    {round(params.rook_open)}")
print(f"#define ROOK_SEMIOPEN     {round(params.rook_semiopen)}")
print(f"#define BISHOP_PAIR_SCORE {round(params.bishop_pair)}")
print(f"// Tempo: {params.tempo:.0f}")

send("quit"); proc.stdin.close(); proc.wait()
print("Done", flush=True)