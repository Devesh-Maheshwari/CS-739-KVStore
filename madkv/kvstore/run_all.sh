#!/bin/bash

cd ~/CS-739-KVStore/madkv

SERVER=devesh@c220g1-031101.wisc.cloudlab.us
ADDR=10.10.1.2:50051

start_server() {
  ssh $SERVER "pkill -f kv_server 2>/dev/null"
  sleep 2
  ssh $SERVER "screen -dmS kvserver bash -c 'cd ~/CS-739-KVStore/madkv && ./kvstore/build/kv_server 0.0.0.0:50051'"
  sleep 2
  ssh $SERVER "ss -tlnp | grep 50051 > /dev/null" && echo "  [OK] Server up" || echo "  [FAIL] Server not listening!"
}

stop_server() {
  ssh $SERVER "pkill -f kv_server 2>/dev/null"
  pkill -f kv_client 2>/dev/null
  sleep 1
}

echo "========== TESTCASES =========="
for i in 1 2 3 4 5; do
  start_server
  echo "--- Testcase $i ---"
  just p1::testcase $i $ADDR
  stop_server
done

echo "========== FUZZ TESTING =========="
start_server
echo "--- Fuzz: 1 client, no conflict ---"
just p1::fuzz 1 no $ADDR
stop_server

start_server
echo "--- Fuzz: 3 clients, no conflict ---"
just p1::fuzz 3 no $ADDR
stop_server

start_server
echo "--- Fuzz: 3 clients, conflict ---"
just p1::fuzz 3 yes $ADDR
stop_server

echo "========== BENCHMARKS: 1 CLIENT =========="
for w in a b c d e f; do
  start_server
  echo "--- Bench: 1 client, workload $w ---"
  just p1::bench 1 $w $ADDR
  stop_server
done

echo "========== BENCHMARKS: SCALING WORKLOAD A =========="
for n in 10 25 40 55 70 85; do
  start_server
  echo "--- Bench: $n clients, workload a ---"
  just p1::bench $n a $ADDR
  stop_server
done

echo "========== BENCHMARKS: SCALING WORKLOAD C =========="
for n in 10 25 40 55 70 85; do
  start_server
  echo "--- Bench: $n clients, workload c ---"
  just p1::bench $n c $ADDR
  stop_server
done

echo "========== BENCHMARKS: SCALING WORKLOAD E =========="
for n in 10 25 40 55 70 85; do
  start_server
  echo "--- Bench: $n clients, workload e ---"
  just p1::bench $n e $ADDR
  stop_server
done

echo ""
echo "========== ALL DONE =========="
echo "Now run: just p1::report"