#!/bin/bash

SERVER=$1

# Client A: puts shared1=valA
(
  echo "PUT shared1 valA"
  sleep 0.1
  echo "GET shared1"
  echo "STOP"
) | ./kvstore/build/kv_client $SERVER &

# Client B: puts shared1=valB
(
  sleep 0.05
  echo "PUT shared1 valB"
  echo "GET shared1"
  echo "STOP"
) | ./kvstore/build/kv_client $SERVER &

wait
