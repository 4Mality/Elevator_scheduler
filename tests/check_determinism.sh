#!/bin/bash
# Runs the scheduler N times and checks the simulation completes every time.
#
# Usage: bash tests/check_determinism.sh <building_file> <people_file> <port> [runs]
#
# Example:
#   bash tests/check_determinism.sh input_files/simple.bldg input_files/simple.ppl 5432 5

BLDG="${1}"
PPL="${2}"
PORT="${3}"
RUNS="${4:-5}"

if [ -z "$BLDG" ] || [ -z "$PPL" ] || [ -z "$PORT" ]; then
    echo "Usage: $0 <building_file> <people_file> <port> [runs]"
    exit 1
fi

if [ ! -f "$BLDG" ]; then echo "ERROR: building file '$BLDG' not found"; exit 1; fi
if [ ! -f "$PPL"  ]; then echo "ERROR: people file '$PPL' not found";    exit 1; fi

BINARY="./scheduler_os"
if [ ! -f "$BINARY" ]; then
    echo "ERROR: $BINARY not found. Run make first."
    exit 1
fi

PASS=0
FAIL=0

for i in $(seq 1 "$RUNS"); do
    echo "--- Run $i / $RUNS ---"

    python3 main.py -b "$BLDG" -p "$PPL" -r /tmp/sim_report_${i}.txt -P "$PORT" &
    OS_PID=$!
    sleep 2

    "$BINARY" "$BLDG" "$PORT" &
    SCHED_PID=$!
    wait $SCHED_PID

    STATUS=""
    for attempt in $(seq 1 30); do
        STATUS=$(curl -s "http://127.0.0.1:${PORT}/Simulation/check")
        if echo "$STATUS" | grep -q "complete\|stopped"; then
            break
        fi
        sleep 2
    done

    kill "$OS_PID" 2>/dev/null
    wait "$OS_PID" 2>/dev/null

    if echo "$STATUS" | grep -q "complete"; then
        echo "  PASS: simulation completed"
        ((PASS++))
    else
        echo "  FAIL: final status was: '$STATUS'"
        ((FAIL++))
    fi

    sleep 2
done

echo ""
echo "Determinism results over $RUNS runs: $PASS passed, $FAIL failed"
[ "$FAIL" -eq 0 ] && exit 0 || exit 1
