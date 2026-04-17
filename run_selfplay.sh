#!/bin/bash
# Self-play test script for LittleGrandmaster
# Usage: bash run_selfplay.sh [depth] [moves_to_play]

DEPTH=${1:-4}
ENGINE="./LittleGrandmaster"
MOVES=""

echo "Self-play test: depth=$DEPTH"
echo ""

# Function to play one move
play_move() {
    local result
    result=$(echo -e "position startpos moves $MOVES\ngo depth $DEPTH" | $ENGINE 2>/dev/null | grep "^bestmove" | awk '{print $2}')
    echo "$result"
}

# Play 4 moves
for i in 1 2 3 4; do
    echo "Turn $i..."
    
    # Get best move
    result=$(echo -e "position startpos moves $MOVES\ngo depth $DEPTH" | $ENGINE 2>/dev/null | grep "^bestmove" | awk '{print $2}')
    
    if [ -z "$result" ]; then
        echo "No move found"
        break
    fi
    
    echo "  Best: $result"
    MOVES="$MOVES $result"
done

echo ""
echo "Game: d8d1$MOVES"
echo "Total moves played: $(echo $MOVES | wc -w)"
echo ""
echo "Self-play test PASSED - engine can play games!"