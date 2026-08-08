#!/bin/sh
# Measure dedicated-server tick cost under bot load.
#
# Starts a dedicated server with the frame benchmark enabled, attaches N bots
# so there are units to simulate and paths to compute, and prints the server's
# report. An idle server does almost no work, so a run without bots tells you
# nothing.
#
# The frame limiter is left ON, because that is how a real server runs: the
# question is not how fast the loop can spin, it is how much of the 20 ms tick
# budget the work consumes. The benchmark excludes SDL_Delay from its samples,
# so the numbers are comparable across throttled and unthrottled runs.
#
# Usage: support/bench/server-bench.sh <build-dir> [ticks] [bots] [port]
#   BENCH_OUT=<path>   where to copy the per-tick CSV (default ./server-ticks.csv)

set -eu

BUILD_DIR="${1:?usage: server-bench.sh <build-dir> [ticks] [bots] [port]}"
TICKS="${2:-5000}"
BOTS="${3:-4}"
PORT="${4:-13337}"

NP="$BUILD_DIR/netpanzer"
[ -x "$NP" ] || { echo "no netpanzer binary at $NP" >&2; exit 1; }

SRC_ROOT=$(cd "$(dirname "$0")/../.." && pwd)
NETPANZER_DATADIR="${NETPANZER_DATADIR:-$SRC_ROOT/data}"
export NETPANZER_DATADIR

WORK=$(mktemp -d)
BOT_PIDS=""
SERVER_PID=""

cleanup() {
  for pid in $BOT_PIDS; do kill "$pid" 2>/dev/null || true; done
  [ -n "$SERVER_PID" ] && kill "$SERVER_PID" 2>/dev/null || true
  rm -rf "$WORK"
}
trap cleanup EXIT INT TERM

echo "== netpanzer server benchmark =="
echo "   build  $BUILD_DIR"
echo "   ticks  $TICKS  (~$((TICKS / 50))s at 50 Hz)"
echo "   bots   $BOTS"
echo "   port   $PORT"
echo

NETPANZER_BENCH_TICKS="$TICKS" \
NETPANZER_BENCH_SLEEP=1 \
NETPANZER_BENCH_CSV="$WORK/server-ticks.csv" \
  "$NP" -d -p "$PORT" --master_server=none < /dev/zero > "$WORK/server.log" 2>&1 &
SERVER_PID=$!

# Give the server time to load its map before the bots knock.
sleep 8

if ! kill -0 "$SERVER_PID" 2>/dev/null; then
  echo "server exited during startup; log tail:" >&2
  grep -av 'netpanzer-server:' "$WORK/server.log" | tail -20 >&2
  exit 1
fi

i=0
while [ "$i" -lt "$BOTS" ]; do
  "$NP" -b "127.0.0.1:$PORT" < /dev/zero > "$WORK/bot$i.log" 2>&1 &
  BOT_PIDS="$BOT_PIDS $!"
  i=$((i + 1))
  sleep 1
done

echo "   $BOTS bots attached; waiting for the server to spend its tick budget"
wait "$SERVER_PID" 2>/dev/null || true
SERVER_PID=""

# The dedicated server prints its console prompt without a newline, so on a
# non-interactive stdin the log becomes one enormous line. Strip it first or
# the report is unreadable.
sed 's/netpanzer-server: //g' "$WORK/server.log" | grep -a '\[bench\]' || {
  echo "no benchmark output; server log tail:" >&2
  grep -av 'netpanzer-server:' "$WORK/server.log" | tail -30 >&2
  exit 1
}

if [ -f "$WORK/server-ticks.csv" ]; then
  OUT="${BENCH_OUT:-$PWD/server-ticks.csv}"
  cp "$WORK/server-ticks.csv" "$OUT"
  echo
  echo "   per-tick samples: $OUT"
fi
