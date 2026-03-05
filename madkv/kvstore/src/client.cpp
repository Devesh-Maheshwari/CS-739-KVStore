#include <grpcpp/grpcpp.h>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <functional>
#include "kvstore.grpc.pb.h"

using namespace std;

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;
using kvstore::KvStore;
using kvstore::Manager;
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
using kvstore::ClusterInfoRequest;
using kvstore::ClusterInfoResponse;

// ─────────────────────────────────────────────────────────────────────────────
// Your original KvStoreClient — two small changes:
//   1. stub_ is public (was private) so PartitionedClient retry lambdas can use it
//   2. ScanRaw() added for fan-out scan across partitions
// ─────────────────────────────────────────────────────────────────────────────
class KvStoreClient {
public:
    unique_ptr<KvStore::Stub> stub_;   // public so PartitionedClient retry can call it

    KvStoreClient(shared_ptr<Channel> channel)
        : stub_(KvStore::NewStub(channel)) {}

    void Put(const string& key, const string& value) {
        PutRequest req;
        req.set_key(key);
        req.set_value(value);
        PutResponse res;
        ClientContext ctx;

        Status status = stub_->Put(&ctx, req, &res);
        if (status.ok()) {
            cout << "PUT " << key << " " << (res.found() ? "found" : "not_found") << endl;
            
        }
        cout.flush();
    }

    void Swap(const string& key, const string& value) {
        SwapRequest req;
        req.set_key(key);
        req.set_value(value);
        SwapResponse res;
        ClientContext ctx;

        Status status = stub_->Swap(&ctx, req, &res);
        if (status.ok()) {
            cout << "SWAP " << key << " "
                 << (res.found() ? res.old_value() : "null") << endl;
        }
        cout.flush();
    }

    void Get(const string& key) {
        GetRequest req;
        req.set_key(key);
        GetResponse res;
        ClientContext ctx;

        Status status = stub_->Get(&ctx, req, &res);
        if (status.ok()) {
            cout << "GET " << key << " "
                 << (res.found() ? res.value() : "null") << endl;
        }
        cout.flush();
    }

    void Scan(const string& start, const string& end) {
        ScanRequest req;
        req.set_key_start(start);
        req.set_key_end(end);
        ScanResponse res;
        ClientContext ctx;

        Status status = stub_->Scan(&ctx, req, &res);
        if (status.ok()) {
            cout << "SCAN " << start << " " << end << " BEGIN" << endl;
            for (const auto& entry : res.entries()) {
                cout << "  " << entry.key() << " " << entry.value() << endl;
            }
            cout << "SCAN END" << endl;
        }
        cout.flush();
    }

    // Returns raw response so PartitionedClient can merge results from all servers
    ScanResponse ScanRaw(const string& start, const string& end) {
        ScanRequest req;
        req.set_key_start(start);
        req.set_key_end(end);
        ScanResponse res;
        ClientContext ctx;
        stub_->Scan(&ctx, req, &res);   // if server is down, res is just empty — ok per spec
        return res;
    }

    void Delete(const string& key) {
        DeleteRequest req;
        req.set_key(key);
        DeleteResponse res;
        ClientContext ctx;

        Status status = stub_->Delete(&ctx, req, &res);
        if (status.ok()) {
            cout << "DELETE " << key << " "
                 << (res.found() ? "found" : "not_found") << endl;
        }
        cout.flush();
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// PartitionedClient — routing layer on top of KvStoreClient.
// Queries manager on startup, then routes each key via hash(key) % N.
// Retries indefinitely on failure as required by the spec.
// ─────────────────────────────────────────────────────────────────────────────
class PartitionedClient {
private:
    int num_partitions_ = 0;
    vector<unique_ptr<KvStoreClient>> clients_;  // index == server_id

    int partitionFor(const string& key) {
        return (int)(hash<string>{}(key) % (size_t)num_partitions_);
    }

    void retryUntil(const string& what, function<bool()> fn) {
        while (true) {
            if (fn()) return;
            cerr << "[client] " << what << " failed, retrying in 500ms..." << endl;
            this_thread::sleep_for(chrono::milliseconds(500));
        }
    }

public:
    explicit PartitionedClient(const string& manager_addr) {
        auto ch  = grpc::CreateChannel(manager_addr, grpc::InsecureChannelCredentials());
        auto mgr = Manager::NewStub(ch);

        ClusterInfoResponse info;
        retryUntil("GetClusterInfo", [&]() {
            ClusterInfoRequest req;
            ClientContext ctx;
            return mgr->GetClusterInfo(&ctx, req, &info).ok();
        });

        num_partitions_ = info.num_partitions();
        clients_.resize(num_partitions_);
        for (const auto& srv : info.servers()) {
            auto c = grpc::CreateChannel(srv.address(), grpc::InsecureChannelCredentials());
            clients_[srv.server_id()] = make_unique<KvStoreClient>(c);
        }
        cerr << "[client] Ready — " << num_partitions_ << " partition(s)" << endl;
    }

    void Put(const string& key, const string& value) {
        int p = partitionFor(key);
        retryUntil("PUT", [&]() {
            PutRequest req; req.set_key(key); req.set_value(value);
            PutResponse res; ClientContext ctx;
            Status s = clients_[p]->stub_->Put(&ctx, req, &res);
            if (s.ok()) {
                // cout << "PUT " << key << " " << value << " " << (res.found() ? "found" : "not_found") << endl;
                cout << "PUT " << key << " " << (res.found() ? "found" : "not_found") << endl;

                cout.flush();
                return true;
            }
            return false;
        });
    }

    void Swap(const string& key, const string& value) {
        int p = partitionFor(key);
        retryUntil("SWAP", [&]() {
            SwapRequest req; req.set_key(key); req.set_value(value);
            SwapResponse res; ClientContext ctx;
            Status s = clients_[p]->stub_->Swap(&ctx, req, &res);
            if (s.ok()) {
                cout << "SWAP " << key << " "
                     << (res.found() ? res.old_value() : "null") << endl;
                cout.flush();
                return true;
            }
            return false;
        });
    }

    void Get(const string& key) {
        int p = partitionFor(key);
        retryUntil("GET", [&]() {
            GetRequest req; req.set_key(key);
            GetResponse res; ClientContext ctx;
            Status s = clients_[p]->stub_->Get(&ctx, req, &res);
            if (s.ok()) {
                cout << "GET " << key << " "
                     << (res.found() ? res.value() : "null") << endl;
                cout.flush();
                return true;
            }
            return false;
        });
    }

    void Scan(const string& start, const string& end) {
        // Fan out to all partitions and merge.
        // Spec relaxes scan consistency — each key checked individually.
        cout << "SCAN " << start << " " << end << " BEGIN" << endl;
        for (auto& c : clients_) {
            auto res = c->ScanRaw(start, end);
            for (const auto& entry : res.entries())
                cout << "  " << entry.key() << " " << entry.value() << endl;
        }
        cout << "SCAN END" << endl;
        cout.flush();
    }

    void Delete(const string& key) {
        int p = partitionFor(key);
        retryUntil("DELETE", [&]() {
            DeleteRequest req; req.set_key(key);
            DeleteResponse res; ClientContext ctx;
            Status s = clients_[p]->stub_->Delete(&ctx, req, &res);
            if (s.ok()) {
                cout << "DELETE " << key << " " << (res.found() ? "found" : "not_found") << endl;
                cout.flush();
                return true;
            }
            return false;
        });
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// main — only change vs P1: --manager_addr arg + PartitionedClient
// The command loop is identical to your original P1 code
// ─────────────────────────────────────────────────────────────────────────────
int main(int argc, char** argv) {
    string manager_addr;
    for (int i = 1; i < argc; i++) {
        string arg = argv[i];
        if (arg == "--manager_addr" && i + 1 < argc) manager_addr = argv[++i];
    }
    if (manager_addr.empty()) {
        cerr << "Usage: " << argv[0] << " --manager_addr <host:port>" << endl;
        return 1;
    }

    PartitionedClient client(manager_addr);

    string line;
    while (getline(cin, line)) {
        istringstream iss(line);
        string cmd;
        iss >> cmd;

        if (cmd == "PUT") {
            string key, value;
            iss >> key >> value;
            client.Put(key, value);
        } else if (cmd == "SWAP") {
            string key, value;
            iss >> key >> value;
            client.Swap(key, value);
        } else if (cmd == "GET") {
            string key;
            iss >> key;
            client.Get(key);
        } else if (cmd == "SCAN") {
            string start, end;
            iss >> start >> end;
            client.Scan(start, end);
        } else if (cmd == "DELETE") {
            string key;
            iss >> key;
            client.Delete(key);
        } else if (cmd == "STOP") {
            cout << "STOP" << endl;
            cout.flush();
            break;
        }
    }

    return 0;
}