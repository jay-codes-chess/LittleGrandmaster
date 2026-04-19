#!/usr/bin/env node
/**
 * LGM Self-play harness - plays a full game between two engine instances.
 */
const { spawn } = require('child_process');

const ENG = "/home/jay/LittleGrandmaster/LittleGrandmaster";
const DEPTH_W = 4;
const DEPTH_B = 5;

async function waitForSubstring(buffer, substr) {
    const start = Date.now();
    while (Date.now() - start < 25000) {
        if (buffer.includes(substr)) return true;
        await new Promise(r => setTimeout(r, 30));
    }
    return false;
}

function getBestmove(buffer) {
    const lines = buffer.split('\n');
    for (let i = lines.length - 1; i >= 0; i--) {
        if (lines[i].includes('bestmove')) {
            const parts = lines[i].trim().split(' ');
            return parts[1];
        }
    }
    return null;
}

async function main() {
    const w = spawn(ENG, [], { cwd: "/home/jay/LittleGrandmaster" });
    const b = spawn(ENG, [], { cwd: "/home/jay/LittleGrandmaster" });
    
    let wb = '', bb = '';
    w.stdout.on('data', d => { wb += d.toString(); });
    b.stdout.on('data', d => { bb += d.toString(); });
    
    const send = (eng, cmd) => { eng.stdin.write(cmd + '\n'); };
    
    // Init both engines
    send(w, 'uci'); send(w, 'isready');
    send(b, 'uci'); send(b, 'isready');
    
    // Wait for init
    while (!wb.includes('readyok') || !bb.includes('readyok')) {
        await new Promise(r => setTimeout(r, 50));
    }
    
    // Clear init buffers after readyok
    wb = ''; bb = '';
    
    const moves = [];
    
    for (let turn = 0; turn < 100; turn++) {
        const isWhite = moves.length % 2 === 0;
        const eng = isWhite ? w : b;
        const buf = isWhite ? wb : bb;
        const depth = isWhite ? DEPTH_W : DEPTH_B;
        
        // Set position
        if (moves.length === 0) {
            send(eng, 'position startpos');
        } else {
            send(eng, `position startpos moves ${moves.join(' ')}`);
        }
        send(eng, `go depth ${depth}`);
        
        // Wait for bestmove
        const found = await waitForSubstring(buf, 'bestmove');
        if (!found) {
            process.stderr.write(`\nTimeout at move ${moves.length + 1}\n`);
            break;
        }
        
        const bm = getBestmove(buf);
        if (!bm) {
            process.stderr.write(`\nNo bestmove parsed at move ${moves.length + 1}\n`);
            break;
        }
        
        moves.push(bm);
        process.stderr.write(`${moves.length}. ${bm} `);
        
        // Clear buffer AFTER reading the bestmove (to not lose state for next move)
        if (isWhite) wb = '';
        else bb = '';
    }
    
    process.stderr.write('\n');
    
    send(w, 'quit'); send(b, 'quit');
    await new Promise(r => setTimeout(r, 200));
    w.kill(); b.kill();
    
    // Output PGN
    console.log('[Event "LGM Self-play"]');
    console.log('[Site "Local"]');
    console.log('[Date "2026.04.19"]');
    console.log('[Round "1"]');
    console.log(`[White "LGM-d${DEPTH_W}"]`);
    console.log(`[Black "LGM-d${DEPTH_B}"]`);
    console.log('[Result "*"]\n');
    
    for (let i = 0; i < moves.length; i += 2) {
        const n = Math.floor(i/2) + 1;
        const wm = moves[i];
        const bm = moves[i+1] || '';
        console.log(`${n}. ${wm}${bm ? ' ' + bm : ''}`);
    }
    console.log('*');
}

main().catch(e => { console.error(e); process.exit(1); });