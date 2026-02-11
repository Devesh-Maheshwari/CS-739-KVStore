#!/bin/bash

SERVER=$1

# Client A: works on a1, a2, a3
(
  echo "PUT a1 valueA1"
  echo "PUT a2 valueA2"
  echo "PUT a3 valueA3"
  echo "GET a1"
  echo "SCAN a1 a3"
  echo "STOP"
) | ./kvstore/build/kv_client $SERVER &

# Client B: works on b1, b2, b3
(
  echo "PUT b1 valueB1"
  echo "PUT b2 valueB2"
  echo "PUT b3 valueB3"
  echo "GET b1"
  echo "SCAN b1 b3"
  echo "STOP"
) | ./kvstore/build/kv_client $SERVER &

wait
