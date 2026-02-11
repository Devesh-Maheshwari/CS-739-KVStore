#!/bin/bash

SERVER=$1

# Client A: writes to shared keys
(
  echo "PUT shared2 initial"
  echo "SWAP shared2 swapped"
  echo "DELETE shared2"
  echo "STOP"
) | ./kvstore/build/kv_client $SERVER &

# Client B: reads shared keys
(
  sleep 0.1
  echo "GET shared2"
  echo "SCAN shared1 shared3"
  echo "STOP"
) | ./kvstore/build/kv_client $SERVER &

wait
