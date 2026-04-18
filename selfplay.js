#!/usr/bin/env node
const { spawn } = require('child_process');
const ENG = "/home/jay/LittleGrandmaster/LittleGrandmaster";

class Engine {
    constructor(name, depth) {
        this.name = name;
        this.depth = depth;
        this.proc = null;
    }
    
    start() {
        return new Promise((resolve, reject) => {
            this.proc = spawn(ENG, [], {
                cwd: "/home/jay/LittleGrandmaster",
                stdio: ['pipe', 'pipe', 'pipe']
            });
            
            let buf = '';
            this.proc.stdout.on('data', (d) => { buf += d.toString(); });
            
            const waitFor = (txt, cb) => {
                const iv = setInterval(() => {
                    if (buf.includes(txt)) {
                        clearInterval(iv);
                        cb();
                    }
                }, 50);
            };
            
            this.send = (cmd) => { this.proc.stdin.write(cmd + '\n'); };
            this.read = (pred) => {
                return new Promise((res) => {
                    const iv = setInterval(() => {
                        const lines = buf.split('\n');
                        for (const l of lines) {
                            if (pred(l)) {
                                clearInterval(iv);
                                res(l);
                                return;
                            }
                        }
                    }, 50);
                    setTimeout(() => { clearInterval(iv); res(null); }, 30000);
                });
            };
            
            this.send("uci");
            waitFor("uciok", () => {
                this.send("ucinewgame");
                this.send("isready");
                waitFor("readyok", resolve);
            });
        });
    }
    
    setpos(moves) {
        if (!moves || moves.length === 0) {
            this.send("position startpos");
        } else {
            this.send(`position startpos moves ${moves.join(' ')}`);
        }
    }
    
    async go() {
        this.send(`go depth ${this.depth}`);
        const line = await this.read(l => l.includes("bestmove"));
        if (!line) return null;
        const parts = line.trim().split(' ');
        return parts[1] || null;
    }
    
    quit() {
        if (this.proc) {
            this.send("quit");
            setTimeout(() => this.proc.kill(), 1000);
        }
    }
}

async function playGame(d1, d2, verbose) {
    const w = new Engine(`LGM-d${d1}`, d1);
    const b = new Engine(`LGM-d${d2}`, d2);
    
    await w.start();
    await b.start();
    
    const moves = [];
    
    try {
        for (let i = 0; i < 200; i++) {
            const eng = moves.length % 2 === 0 ? w : b;
            eng.setpos(moves.length > 0 ? moves : null);
            const mv = await eng.go();
            if (!mv) break;
            moves.push(mv);
            if (verbose && moves.length <= 20) {
                console.log(`  ${moves.length}. ${mv}`);
            }
            const other = eng === w ? b : w;
            other.setpos(moves.length > 0 ? moves : null);
        }
    } finally {
        w.quit();
        b.quit();
    }
    
    return moves;
}

function toPGN(moves, wn, bn) {
    let pgn = `[Event "Test"]\n[Site "Local"]\n[Date "2026.04.18"]\n[Round "1"]\n[White "${wn}"]\n[Black "${bn}"]\n[Result "*"]\n\n`;
    for (let i = 0; i < moves.length; i += 2) {
        const n = i/2 + 1;
        const wm = moves[i];
        const bm = moves[i+1] || '';
        pgn += `${n}. ${wm}${bm ? ' ' + bm : ''}\n`;
    }
    pgn += "*\n";
    return pgn;
}

async function main() {
    console.log("Playing LGM-d3 vs LGM-d4...");
    const moves = await playGame(3, 4, true);
    console.log(`\nGame ended: ${moves.length} moves`);
    console.log(toPGN(moves, "LGM-d3", "LGM-d4"));
}

main().catch(e => { console.error(e); process.exit(1); });