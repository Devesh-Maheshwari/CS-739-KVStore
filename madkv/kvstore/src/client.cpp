#include <grpcpp/grpcpp.h>
#include <iostream>
#include <sstream>
#include <string>
#include "kvstore.grpc.pb.h"

using namespace std;

using grpc::Channel;
using grpc::ClientContext;
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

class KvStoreClient {
private:
    unique_ptr<KvStore::Stub> stub_;

public:
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

int main(int argc, char** argv) {
    if (argc < 2) {
        cerr << "Usage: " << argv[0] << " <server_address>" << endl;
        return 1;
    }

    string server_address(argv[1]);
    KvStoreClient client(grpc::CreateChannel(server_address, grpc::InsecureChannelCredentials()));

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
