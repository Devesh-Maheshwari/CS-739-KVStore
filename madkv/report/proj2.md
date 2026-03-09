# CS 739 MadKV Project 2

**Group members**: Devesh Maheshwari `dmaheshwar22@wisc.edu`

## Design Walkthrough

Our implementation uses three components: a cluster manager, multiple KV servers, and clients. The manager is launched first with a list of server addresses and assigns each server a partition ID equal to its server ID. Clients query the manager once on startup to get the full partition map, then connect directly to all servers.

For durability, each server maintains a SQLite-based write-ahead log (WAL) under its backer_path directory. Every mutating operation (Put, Swap, Delete) writes to the WAL before modifying the in-memory map and acknowledging the client. On restart, the server replays the WAL in order to reconstruct the in-memory state.

Partitioning uses std::hash<string>(key) % N to route each key to a server. This is a simple hash-based static partitioning — each server owns exactly 1/N of the keyspace. Scans fan out to all servers and merge results locally.

Error handling uses indefinite retry loops with 500ms backoff: servers retry connecting to the manager, and clients retry failed RPCs until the server recovers.

## Self-provided Testcase

You will run the described testcase during demo time.

### Explanations

Our testcase launches a 3-server cluster and performs Put/Swap operations on keys a, b, c, d. We then verify Gets and Scans succeed. Next we kill the server owning key "b" (server 1) and confirm that Gets on unaffected keys still work while a Get on "b" hangs indefinitely. After restarting server 1 with the same backer_path, we verify that "b" returns its latest value (BETA from the prior Swap), proving WAL recovery works correctly.

## Fuzz Testing

<u>Parsed the following fuzz testing results:</u>

num_servers | crashing | outcome
:-: | :-: | :-:
3 | no | PASSED
3 | yes | PASSED
5 | yes | PASSED

You will run a crashing/recovering fuzz test during demo time.

### Comments

Scenario A (3 partitions, no crash): All 25000 ops completed successfully across 5 concurrent clients with conflicting keys. Linearizability was maintained.

Scenario B (3 partitions, crash server 1): The fuzzer hung as expected when server 1 was killed. After restart, server 1 replayed its WAL and clients automatically resumed. Final result: PASSED.

Scenario C (5 partitions, crash servers 1+2): Same behavior with two simultaneous failures. Both servers recovered via WAL replay and the fuzzer completed successfully.

## YCSB Benchmarking

<u>10 clients throughput/latency across workloads & number of partitions:</u>

![ten-clients](plots-p2/ycsb-ten-clients.png)

<u>Agg. throughput trend vs. number of clients w/ and w/o partitioning:</u>

![tput-trend](plots-p2/ycsb-tput-trend.png)

### Comments

For 10-client workloads, throughput increases with more partitions on write-heavy workloads (A, F) due to reduced contention per server. Read-heavy workloads (C, D) show less improvement since reads don't contend on locks. Workload E (scan-heavy) shows similar throughput across partition counts since scans fan out to all servers regardless.

For client scaling on workload A, 5 partitions consistently outperforms 1 partition at higher client counts (20-30 clients) because the load is spread across 5 servers. Both plateau around 20 clients suggesting the bottleneck shifts from concurrency to network/disk I/O at that point.

## Additional Discussion

*OPTIONAL: add extra discussions if applicable*

