import subprocess, sys

ENGINE = "./LittleGrandmaster"
proc = subprocess.Popen([ENGINE], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                        text=True, bufsize=1)

def send(cmd):
    proc.stdin.write(cmd + "\n"); proc.stdin.flush()

def read_until(stop):
    lines = []
    while True:
        line = proc.stdout.readline()
        if not line: break
        line = line.rstrip()
        lines.append(line)
        if stop(line): break
    return lines

send("uci")
read_until(lambda l: "uciok" in l)
send("isready")
read_until(lambda l: l == "readyok")
print("Engine ready")

# Test: position + go depth 1
send("position startpos")
send("go depth 1 movetime 100")
lines = read_until(lambda l: l.startswith("bestmove"))
print("Result lines:", len(lines))
for line in lines:
    if "bestmove" in line:
        print("Bestmove:", line)

send("quit")
proc.stdin.close()
proc.wait()
print("Done")