#!/bin/bash
# Runs the scheduler N times against a live simulation OS and checks that
# the simulation reaches "Simulation is complete." every time.
#
# Usage: bash tests/check_determinism.sh <building_file> <port> <os_start_cmd> [runs]
#
# Example:
#   bash tests/check_determinism.sh tests/simple.bldg 5432 "python3 elevator_os.py" 5
#
# <os_start_cmd> is the shell command that starts the simulation OS.
# The script appends the port and building file to that command automatically.

BLDG="${1}"
PORT="${2}"
OS_CMD="${3}"
RUNS="${4:-5}"

if [ -z "$BLDG" ] || [ -z "$PORT" ] || [ -z "$OS_CMD" ]; then
    echo "Usage: $0 <building_file> <port> <os_start_cmd> [runs]"
    exit 1
fi

if [ ! -f "$BLDG" ]; then
    echo "ERROR: building file '$BLDG' not found"
    exit 1
fi

BINARY="./scheduler_os"
if [ ! -f "$BINARY" ]; then
    echo "ERROR: $BINARY not found. Run make first."
    exit 1
fi

PASS=0
FAIL=0

for i in $(seq 1 "$RUNS"); do
    echo "--- Run $i / $RUNS ---"

    # start the simulation OS in the background
    $OS_CMD "$PORT" "$BLDG" &
    OS_PID=$!
    sleep 1  # give the OS a moment to bind its port

    # run the scheduler; give it up to 5 hours per spec but cap test runs low
    timeout 120 "$BINARY" "$BLDG" "$PORT" &
    SCHED_PID=$!
    wait $SCHED_PID

    # poll until the simulation reports done or we time out
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

    sleep 1  # let the port close before next run
done

echo ""
echo "Determinism results over $RUNS runs: $PASS passed, $FAIL failed"
[ "$FAIL" -eq 0 ] && exit 0 || exit 1
