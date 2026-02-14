# CS 739 MadKV Project 1

**Group members**: Devesh Maheshwari `dmaheshwar22@wisc.edu`, Cole Bollig `cabollig@wisc.edu`

## Design Walkthrough

We implemented a simple client/server KVStore in c++ using the
gRPC library to handle RPC stub creation. The server is a simple
single threaded process that handles client RPCs to manage a map
of strings. The simple client that reads commands from standard
input and executes the associated RPC to the server. We implemented
the following RPCs:

1. PUT: Store key/value pair
   1. Client request message \{Key: string, Value: string\}
   2. Server response message \{Key-Found: boolean\}
2. SWAP: Store key/value pair and return old value
   1. Client request message \{Key: string, Value: string\}
   2. Server response message \{Key-Found: boolean, Old-Value: string\}
3. GET: Recieve stored keys value
   1. Client request message \{Key: string\}
   2. Server response message \{Key-Found: boolean, Value: string\}
4. SCAN: Return key/value pairs within specified key range
   1. Client request message \{Range-Begin: string, Range-End: string\}
   2. Server response message \[\{Key: string, Value: string\} ... \{Key: string, Value: string\}\] (vector of key/value pairs)
5. DELETE: Remove key from store
   1. Client request message \{Key: string\}
   2. Server response message \{Key-Found: boolean\}

## Self-provided Testcases

<u>Found the following testcase results:</u> 1, 2, 3, 4, 5

You will run some testcases during demo time.

### Explanations

1. Test 1 verifies the basic functionality of each RPC command with
   a single client and server.
2. Test 2 checks basic RPC functionality with non-existent key references
   with a single client and server.
3. Test 3 concurrently runs two clients talking to one server. These clients
   both run three PUT, one GET, and one SCAN RPCs on non-colliding keys.
4. Test 4 concurrently runs two clients talking to one server such that
   client 1 writes to a key, client two writes a new value to the same key,
   and both clients run the GET RPC on the shared key.
5. Test 5 concurrently runs two clients talking to one server testing the
   SWAP, SCAN, and DELETE RPCs on a shared key.

## Fuzz Testing

<u>Parsed the following fuzz testing results:</u>

num_clis | conflict | outcome
:-: | :-: | :-:
1 | no | PASSED
3 | no | PASSED
3 | yes | PASSED

You will run a multi-client conflicting-keys fuzz test during demo time.

### Comments

We do not think there is much to say on this given the fuzzer passes.

## YCSB Benchmarking

<u>Single-client throughput/latency across workloads:</u>

![single-cli](plots-p1/ycsb-single-cli.png)

<u>Agg. throughput trend vs. number of clients:</u>

![tput-trend](plots-p1/ycsb-tput-trend.png)

<u>Avg. latency trend vs. number of clients:</u>

![lats-trend](plots-p1/ycsb-lats-trend.png)

### Comments

1. Based on the single client workloads we can see that the SCAN operation
   reduces throughput and increases latency as shown by the difference in workload e, which does scan operations, compared to the rest. The reduction 
   of throughput and increase in latency during SCANs makes sense as more information must be sent back to the client which is not streamed in our code.
2. The aggregate throuphput appears to be the same across multiple clients
   with slight degregation as the number of clients increase. This makes sense
   as the total throughput is limited by the single server not the number of
   available clients.
3. The final graphs show scalability concerns as all operations have an increase
   in latency as the number of clients for a workload increase.

## Additional Discussion

*OPTIONAL: add extra discussions if applicable*
