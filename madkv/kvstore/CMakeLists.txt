cmake_minimum_required(VERSION 3.15)
project(kvstore CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find packages
find_package(Threads REQUIRED)
find_package(Protobuf REQUIRED)
find_package(gRPC REQUIRED)

# Proto file
set(PROTO_PATH "${CMAKE_CURRENT_SOURCE_DIR}/proto")
set(PROTO_FILE "${PROTO_PATH}/kvstore.proto")
set(GENERATED_PROTOBUF_PATH "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY ${GENERATED_PROTOBUF_PATH})

# Generate gRPC C++ files
set(PROTO_SRCS "${GENERATED_PROTOBUF_PATH}/kvstore.pb.cc")
set(PROTO_HDRS "${GENERATED_PROTOBUF_PATH}/kvstore.pb.h")
set(GRPC_SRCS "${GENERATED_PROTOBUF_PATH}/kvstore.grpc.pb.cc")
set(GRPC_HDRS "${GENERATED_PROTOBUF_PATH}/kvstore.grpc.pb.h")

add_custom_command(
    OUTPUT ${PROTO_SRCS} ${PROTO_HDRS} ${GRPC_SRCS} ${GRPC_HDRS}
    COMMAND protobuf::protoc
    ARGS --grpc_out=${GENERATED_PROTOBUF_PATH}
         --cpp_out=${GENERATED_PROTOBUF_PATH}
         -I${PROTO_PATH}
         --plugin=protoc-gen-grpc=$<TARGET_FILE:gRPC::grpc_cpp_plugin>
         ${PROTO_FILE}
    DEPENDS ${PROTO_FILE}
)

# Server executable
add_executable(kv_server
    src/server.cpp
    ${PROTO_SRCS}
    ${GRPC_SRCS}
)

target_include_directories(kv_server PRIVATE ${GENERATED_PROTOBUF_PATH})
target_link_libraries(kv_server
    gRPC::grpc++
    protobuf::libprotobuf
    Threads::Threads
)

# Client executable
add_executable(kv_client
    src/client.cpp
    ${PROTO_SRCS}
    ${GRPC_SRCS}
)

target_include_directories(kv_client PRIVATE ${GENERATED_PROTOBUF_PATH})
target_link_libraries(kv_client
    gRPC::grpc++
    protobuf::libprotobuf
    Threads::Threads
)
