set working-directory := '..'

# list project 2 just recipes
default:
    @just --list p2 --unsorted

# install extra dependencies of your KV code
deps:
    @echo "No extra deps (CMake + g++ + grpc/protobuf + sqlite3 assumed available)."

# build your executables in release mode
build:
    cmake -S kvstore -B kvstore/build -DCMAKE_BUILD_TYPE=Release
    cmake --build kvstore/build --parallel

# clean the build of your executables
clean:
    rm -rf kvstore/build

# run your KV store manager
manager man_port="3666" servers="127.0.0.1:3777,127.0.0.1:3778,127.0.0.1:3779":
    ./kvstore/build/kv_manager \
        --man_listen 0.0.0.0:{{man_port}} \
        --servers {{servers}}

# run your KV store server
server id="0" manager="127.0.0.1:3666" api_port="3777" backer_path="./backer.s0":
    ./kvstore/build/kv_server \
        --server_id {{id}} \
        --manager_addr {{manager}} \
        --api_listen 0.0.0.0:{{api_port}} \
        --backer_path {{backer_path}}

# run your KV store client in stdin/out interface mode
client manager="127.0.0.1:3666":
    ./kvstore/build/kv_client \
        --manager_addr {{manager}}

# kill all processes of your KV store system
kill:
    -pkill -f "kv_manager" >/dev/null 2>&1
    -pkill -f "kv_server" >/dev/null 2>&1
    -pkill -f "kv_client" >/dev/null 2>&1

python_run := if `which uv || true` != "" { "uv run" } else { "python3" }
tmpdir_prefix := "/tmp/madkv-p2"

# launch the KV service components on node (see README for conventions)
service node="s0" manager="127.0.0.1:3666" servers="127.0.0.1:3777,127.0.0.1:3778,127.0.0.1:3779" backer_prefix="./backer":
    just p2::build
    just utils::build
    cargo run -p runner -r --bin service -- \
        --server-just-args p2::server \
            $({{python_run}} justmod/xtract.py partid "{{node}}") \
            "{{manager}}" \
            $({{python_run}} justmod/xtract.py portof "{{servers}}" "{{node}}") \
            "{{backer_prefix}}.{{node}}" \
        --manager-just-args p2::manager \
            $({{python_run}} justmod/xtract.py portof "{{manager}}" "{{node}}") \
            "{{servers}}" \
        --node-id "{{node}}"

# ensure a subdir under 'tmp/' exists
tmpdir subdir:
    mkdir -p "{{tmpdir_prefix}}/{{subdir}}"

# run a fuzz testing scenario
fuzz nservers="1" crashing="no" manager="127.0.0.1:3666": (tmpdir "fuzz")
    just p2::build
    just utils::build
    cargo run -p runner -r --bin fuzzer -- \
        --num-clis 5 \
        --conflict \
        --client-just-args p2::client "{{manager}}" \
        | tee "{{tmpdir_prefix}}/fuzz/fuzz-{{nservers}}-{{crashing}}.log"
    just p2::kill

# run a YCSB benchmark workload
bench nclis wload nservers="1" manager="127.0.0.1:3666": (tmpdir "bench")
    just p2::build
    just utils::build
    just utils::ycsb
    cargo run -p runner -r --bin bencher -- \
        --num-clis "{{nclis}}" \
        --workload "{{wload}}" \
        --client-just-args p2::client "{{manager}}" \
        | tee "{{tmpdir_prefix}}/bench/bench-{{nclis}}-{{wload}}-{{nservers}}.log"
    just p2::kill

# generate .md report template from existing results (wip)
report:
    {{python_run}} sumgen/proj2.py