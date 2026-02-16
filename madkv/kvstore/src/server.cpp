#include <grpcpp/grpcpp.h>
#include <map>
#include <mutex>
#include <string>
#include <iostream>
#include "kvstore.grpc.pb.h"

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;
using kvstore::KvStore;
using kvstore::PutRequest;
using kvstore::PutResponse;
using kvstore::SwapRequest;
using kvstore::SwapResponse;
using kvstore::GetRequest;
using kvstore::GetResponse;
using kvstore::ScanRequest;
using kvstore::ScanResponse;
using kvstore::DeleteRequest;
using kvstore::DeleteResponse;

class KvStoreService final : public KvStore::Service {
private:
    std::map<std::string, std::string> store_;
    std::mutex mu_;

public:
    Status Put(ServerContext* ctx, const PutRequest* req, PutResponse* res) override {
        std::lock_guard<std::mutex> lock(mu_);
        auto it = store_.find(req->key());
        bool found = (it != store_.end());
        store_[req->key()] = req->value();
        res->set_found(found);
        return Status::OK;
    }

    Status Swap(ServerContext* ctx, const SwapRequest* req, SwapResponse* res) override {
        std::lock_guard<std::mutex> lock(mu_);
        auto it = store_.find(req->key());
        if (it != store_.end()) {
            res->set_found(true);
            res->set_old_value(it->second);
            it->second = req->value();
        } else {
            res->set_found(false);
            store_[req->key()] = req->value();
        }
        return Status::OK;
    }

    Status Get(ServerContext* ctx, const GetRequest* req, GetResponse* res) override {
        std::lock_guard<std::mutex> lock(mu_);
        auto it = store_.find(req->key());
        if (it != store_.end()) {
            res->set_found(true);
            res->set_value(it->second);
        } else {
            res->set_found(false);
        }
        return Status::OK;
    }

    Status Scan(ServerContext* ctx, const ScanRequest* req, ScanResponse* res) override {
        std::lock_guard<std::mutex> lock(mu_);
        auto start = store_.lower_bound(req->key_start());
        auto end = store_.upper_bound(req->key_end());

        for (auto it = start; it != end; ++it) {
            auto* entry = res->add_entries();
            entry->set_key(it->first);
            entry->set_value(it->second);
        }
        return Status::OK;
    }

    Status Delete(ServerContext* ctx, const DeleteRequest* req, DeleteResponse* res) override {
        std::lock_guard<std::mutex> lock(mu_);
        auto it = store_.find(req->key());
        if (it != store_.end()) {
            store_.erase(it);
            res->set_found(true);
        } else {
            res->set_found(false);
        }
        return Status::OK;
    }
};

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <listen_address>" << std::endl;
        return 1;
    }

    std::string server_address(argv[1]);
    KvStoreService service;

    ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);

    std::unique_ptr<Server> server(builder.BuildAndStart());
    std::cout << "Server listening on " << server_address << std::endl;
    server->Wait();

    return 0;
}
