#!/bin/bash

cd ~/CS-739-KVStore/madkv

MANAGER_ADDR="127.0.0.1:3666"
SERVERS_1="127.0.0.1:3777"
SERVERS_3="127.0.0.1:3777,127.0.0.1:3778,127.0.0.1:3779"
SERVERS_5="127.0.0.1:3777,127.0.0.1:3778,127.0.0.1:3779,127.0.0.1:3780,127.0.0.1:3781"

# ─────────────────────────────────────────────────────────────────────────────
# Helpers
# ─────────────────────────────────────────────────────────────────────────────
start_cluster() {
    local n=$1
    local servers=$2

    just p2::kill 2>/dev/null
    sleep 1
    rm -rf ./backer.s*

    just p2::manager 3666 "$servers" &
    sleep 1

    local ports=(3777 3778 3779 3780 3781)
    local backers=(s0 s1 s2 s3 s4)
    for ((i=0; i<n; i++)); do
        just p2::server $i $MANAGER_ADDR ${ports[$i]} ./backer.${backers[$i]} &
    done

    echo "  Waiting for $n-partition cluster..."
    for attempt in $(seq 1 30); do
        result=$(echo "STOP" | timeout 3 ./kvstore/build/kv_client --manager_addr $MANAGER_ADDR 2>&1)
        if echo "$result" | grep -q "Ready"; then
            echo "  [OK] Cluster ready"
            return 0
        fi
        sleep 1
    done
    echo "  [FAIL] Cluster did not start"
    return 1
}

stop_cluster() {
    # just p2::kill 2>/dev/null
    pkill -f "kv_manager" 2>/dev/null || true
    pkill -f "kv_server" 2>/dev/null || true
    pkill -f "kv_client" 2>/dev/null || true
    sleep 1
}

# ─────────────────────────────────────────────────────────────────────────────
# FUZZ: 3 servers, no crash
# ─────────────────────────────────────────────────────────────────────────────
# echo "========== [fuzz 3 servers healthy] =========="
# start_cluster 3 "$SERVERS_3"
# just p2::fuzz 3 no $MANAGER_ADDR
# stop_cluster

# # ─────────────────────────────────────────────────────────────────────────────
# # FUZZ: 3 servers, crash server 1 midway
# # ─────────────────────────────────────────────────────────────────────────────
# echo "========== [fuzz 3 servers crashing] =========="
# start_cluster 3 "$SERVERS_3"
# just p2::fuzz 3 yes $MANAGER_ADDR &
# FUZZ_PID=$!
# sleep 8
# echo "  Killing server 1..."
# pkill -f "backer.s1"
# sleep 4
# echo "  Restarting server 1..."
# just p2::server 1 $MANAGER_ADDR 3778 ./backer.s1 &
# wait $FUZZ_PID
# stop_cluster

# # ─────────────────────────────────────────────────────────────────────────────
# # FUZZ: 5 servers, crash servers 1+2 midway
# # ─────────────────────────────────────────────────────────────────────────────
# echo "========== [fuzz 5 servers crashing] =========="
# start_cluster 5 "$SERVERS_5"
# just p2::fuzz 5 yes $MANAGER_ADDR &
# FUZZ_PID=$!
# sleep 8
# echo "  Killing servers 1 and 2..."
# pkill -f "backer.s1"
# pkill -f "backer.s2"
# sleep 4
# echo "  Restarting servers 1 and 2..."
# just p2::server 1 $MANAGER_ADDR 3778 ./backer.s1 &
# just p2::server 2 $MANAGER_ADDR 3779 ./backer.s2 &
# wait $FUZZ_PID
# stop_cluster

# ─────────────────────────────────────────────────────────────────────────────
# YCSB: 10 clients, workloads A-F, 1/3/5 partitions
# ─────────────────────────────────────────────────────────────────────────────
echo "========== YCSB: 10 clients, all workloads, 1/3/5 partitions =========="
for w in a b c d e f; do
    for nservers in 1 3 5; do
        if   [ $nservers -eq 1 ]; then servers="$SERVERS_1"
        elif [ $nservers -eq 3 ]; then servers="$SERVERS_3"
        else                           servers="$SERVERS_5"
        fi
        echo "--- [ycsb-$w $nservers parts 10 clis] ---"
        start_cluster $nservers "$servers"
        just p2::bench 10 $w $nservers $MANAGER_ADDR
        stop_cluster
    done
done

# ─────────────────────────────────────────────────────────────────────────────
# YCSB: Workload A, 1 partition, scale clients 1/20/30
# ─────────────────────────────────────────────────────────────────────────────
echo "========== YCSB: Workload A, 1 partition, scaling clients =========="
for n in 1 20 30; do
    echo "--- [ycsb-a 1 parts $n clis] ---"
    start_cluster 1 "$SERVERS_1"
    just p2::bench $n a 1 $MANAGER_ADDR
    stop_cluster
done

# ─────────────────────────────────────────────────────────────────────────────
# YCSB: Workload A, 5 partitions, scale clients 1/20/30
# ─────────────────────────────────────────────────────────────────────────────
echo "========== YCSB: Workload A, 5 partitions, scaling clients =========="
for n in 1 20 30; do
    echo "--- [ycsb-a 5 parts $n clis] ---"
    start_cluster 5 "$SERVERS_5"
    just p2::bench $n a 5 $MANAGER_ADDR
    stop_cluster
done

echo ""
echo "========== ALL DONE =========="
echo "Now run: just p2::report"