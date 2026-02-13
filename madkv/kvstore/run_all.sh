#!/bin/bash

SERVER_IP=$1
PORT=$2
ADDR="${SERVER_IP}:${PORT}"

if [ -z "$SERVER_IP" ] || [ -z "$PORT" ]; then
  echo "Usage: ./run_all_p1.sh <server_ip> <port>"
  exit 1
fi

echo "Using server address: $ADDR"

start_server() {
  echo "Starting fresh server..."
  just p1::service 0.0.0.0:$PORT > /tmp/server.log 2>&1 &
  SERVER_PID=$!
  sleep 3
}

stop_server() {
  echo "Stopping server..."
  just p1::kill
  kill $SERVER_PID 2>/dev/null
  sleep 2
}

# ------------------------
# Testcases
# ------------------------

for i in 1 2 3 4 5; do
  start_server
  echo "Running testcase $i..."
  just p1::testcase $i $ADDR
  stop_server
done

# ------------------------
# Fuzz
# ------------------------

start_server
just p1::fuzz 1 no $ADDR
stop_server

start_server
just p1::fuzz 3 no $ADDR
stop_server

start_server
just p1::fuzz 3 yes $ADDR
stop_server

# ------------------------
# Benchmarks
# ------------------------

# 1 client workloads
for w in a b c d e f; do
  start_server
  just p1::bench 1 $w $ADDR
  stop_server
done

# Scaling workloads
for n in 10 25 40 55 70 85; do
  start_server
  just p1::bench $n a $ADDR
  stop_server
done

for n in 10 25 40 55 70 85; do
  start_server
  just p1::bench $n c $ADDR
  stop_server
done

for n in 10 25 40 55 70 85; do
  start_server
  just p1::bench $n e $ADDR
  stop_server
done

echo "All tests, fuzz, and benchmarks completed."
echo "You can now run: just p1::report"
