#include <grpcpp/grpcpp.h>
#include <iostream>
#include <vector>
#include <string>
#include <mutex>
#include <sstream>
#include "kvstore.grpc.pb.h"

using namespace std;
using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;
using kvstore::Manager;
using kvstore::RegisterRequest;
using kvstore::RegisterResponse;
using kvstore::ClusterInfoRequest;
using kvstore::ClusterInfoResponse;
using kvstore::ServerInfo;

class ManagerService final : public Manager::Service {
private:
    int num_partitions_;
    vector<string> server_addresses_;   // index == server_id, pre-filled from CLI
    vector<bool> registered_;
    mutex mu_;

public:
    ManagerService(int n, vector<string> addresses)
        : num_partitions_(n), server_addresses_(addresses), registered_(n, false) {}

    Status RegisterServer(ServerContext* ctx,
                          const RegisterRequest* req,
                          RegisterResponse* res) override {
        lock_guard<mutex> lock(mu_);
        int id = req->server_id();
        if (id < 0 || id >= num_partitions_) {
            return Status(grpc::INVALID_ARGUMENT, "bad server_id");
        }
        // Override address with what the server reports (handles 0.0.0.0 -> actual IP)
        // But we already have it from CLI; just mark as registered
        registered_[id] = true;
        cerr << "[manager] Server " << id << " registered from " 
             << server_addresses_[id] << endl;

        res->set_num_partitions(num_partitions_);
        res->set_partition_id(id);
        return Status::OK;
    }

    Status GetClusterInfo(ServerContext* ctx,
                          const ClusterInfoRequest* req,
                          ClusterInfoResponse* res) override {
        lock_guard<mutex> lock(mu_);
        // Block until all servers registered
        for (int i = 0; i < num_partitions_; i++) {
            if (!registered_[i]) {
                return Status(grpc::UNAVAILABLE, "not all servers registered yet");
            }
        }
        res->set_num_partitions(num_partitions_);
        for (int i = 0; i < num_partitions_; i++) {
            auto* s = res->add_servers();
            s->set_server_id(i);
            s->set_address(server_addresses_[i]);
        }
        return Status::OK;
    }
};

int main(int argc, char** argv) {
    string man_listen, servers_csv;

    for (int i = 1; i < argc; i++) {
        string arg = argv[i];
        if (arg == "--man_listen" && i+1 < argc) man_listen = argv[++i];
        else if (arg == "--servers" && i+1 < argc) servers_csv = argv[++i];
    }

    // Parse comma-separated server addresses
    vector<string> addresses;
    stringstream ss(servers_csv);
    string addr;
    while (getline(ss, addr, ',')) addresses.push_back(addr);

    int n = addresses.size();
    cerr << "[manager] Expecting " << n << " servers, listening on " << man_listen << endl;

    ManagerService service(n, addresses);
    ServerBuilder builder;
    builder.AddListeningPort(man_listen, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);

    auto server = builder.BuildAndStart();
    server->Wait();
    return 0;
}