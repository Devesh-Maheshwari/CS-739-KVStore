#include <grpcpp/grpcpp.h>
#include <map>
#include <mutex>
#include <string>
#include <iostream>
#include <filesystem>
#include <thread>
#include <chrono>
#include <sqlite3.h>
#include "kvstore.grpc.pb.h"

using namespace std;

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;
using grpc::Channel;
using grpc::ClientContext;
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
using kvstore::RegisterRequest;
using kvstore::RegisterResponse;

class KvStoreService final : public KvStore::Service {
private:
    map<string, string> store_;
    mutex mu_;
    sqlite3* db_ = nullptr;

    void initDB(const string& db_path) {
        sqlite3_open(db_path.c_str(), &db_);
        sqlite3_exec(db_, "PRAGMA journal_mode=WAL;", nullptr, nullptr, nullptr);
        // sqlite3_exec(db_, "PRAGMA synchronous=NORMAL;", nullptr, nullptr, nullptr);
        sqlite3_exec(db_, "PRAGMA synchronous=OFF;", nullptr, nullptr, nullptr);
        sqlite3_exec(db_, "PRAGMA cache_size=10000;", nullptr, nullptr, nullptr);
        sqlite3_exec(db_,
            "CREATE TABLE IF NOT EXISTS wal ("
            "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "  op TEXT NOT NULL,"
            "  key TEXT NOT NULL,"
            "  value TEXT NOT NULL DEFAULT ''"
            ");",
            nullptr, nullptr, nullptr);
    }

    void appendLog(const string& op, const string& key, const string& value = "") {
        sqlite3_stmt* stmt;
        sqlite3_prepare_v2(db_,
            "INSERT INTO wal (op, key, value) VALUES (?, ?, ?);",
            -1, &stmt, nullptr);
        sqlite3_bind_text(stmt, 1, op.c_str(),    -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, key.c_str(),   -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, value.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }

    void replayLog() {
        sqlite3_stmt* stmt;
        sqlite3_prepare_v2(db_,
            "SELECT op, key, value FROM wal ORDER BY id ASC;",
            -1, &stmt, nullptr);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            string op    = (const char*)sqlite3_column_text(stmt, 0);
            string key   = (const char*)sqlite3_column_text(stmt, 1);
            string value = (const char*)sqlite3_column_text(stmt, 2);
            if (op == "PUT" || op == "SWAP") {
                store_[key] = value;
            } else if (op == "DELETE") {
                store_.erase(key);
            }
        }
        sqlite3_finalize(stmt);
        cerr << "[server] Replayed WAL, store size: " << store_.size() << endl;
    }

public:
    KvStoreService(const string& backer_path) {
        filesystem::create_directories(backer_path);
        string db_path = backer_path + "/wal.db";
        initDB(db_path);
        replayLog();
    }

    ~KvStoreService() {
        if (db_) sqlite3_close(db_);
    }

    Status Put(ServerContext* ctx, const PutRequest* req, PutResponse* res) override {
        lock_guard<mutex> lock(mu_);
        appendLog("PUT", req->key(), req->value());
        bool found = store_.count(req->key()) > 0;
        store_[req->key()] = req->value();
        res->set_found(found);
        return Status::OK;
    }

    Status Swap(ServerContext* ctx, const SwapRequest* req, SwapResponse* res) override {
        lock_guard<mutex> lock(mu_);
        auto it = store_.find(req->key());
        if (it != store_.end()) {
            appendLog("SWAP", req->key(), req->value());
            res->set_found(true);
            res->set_old_value(it->second);
            it->second = req->value();
        } else {
            appendLog("PUT", req->key(), req->value());
            res->set_found(false);
            store_[req->key()] = req->value();
        }
        return Status::OK;
    }

    Status Get(ServerContext* ctx, const GetRequest* req, GetResponse* res) override {
        lock_guard<mutex> lock(mu_);
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
        lock_guard<mutex> lock(mu_);
        auto start = store_.lower_bound(req->key_start());
        auto end   = store_.upper_bound(req->key_end());
        for (auto it = start; it != end; ++it) {
            auto* entry = res->add_entries();
            entry->set_key(it->first);
            entry->set_value(it->second);
        }
        return Status::OK;
    }

    Status Delete(ServerContext* ctx, const DeleteRequest* req, DeleteResponse* res) override {
        lock_guard<mutex> lock(mu_);
        auto it = store_.find(req->key());
        if (it != store_.end()) {
            appendLog("DELETE", req->key());
            store_.erase(it);
            res->set_found(true);
        } else {
            res->set_found(false);
        }
        return Status::OK;
    }
};

int main(int argc, char** argv) {
    string manager_addr, api_listen, backer_path;
    int server_id = 0;

    for (int i = 1; i < argc; i++) {
        string arg = argv[i];
        if      (arg == "--manager_addr" && i+1 < argc) manager_addr = argv[++i];
        else if (arg == "--api_listen"   && i+1 < argc) api_listen   = argv[++i];
        else if (arg == "--server_id"    && i+1 < argc) server_id    = stoi(argv[++i]);
        else if (arg == "--backer_path"  && i+1 < argc) backer_path  = argv[++i];
    }

    if (api_listen.empty() || backer_path.empty()) {
        cerr << "Usage: server --manager_addr <addr> --api_listen <addr> "
             << "--server_id <id> --backer_path <path>" << endl;
        return 1;
    }

    // Register with manager, retry until it's up
    if (!manager_addr.empty()) {
        auto channel = grpc::CreateChannel(manager_addr, grpc::InsecureChannelCredentials());
        auto stub = Manager::NewStub(channel);
        while (true) {
            RegisterRequest req;
            req.set_server_id(server_id);
            req.set_address(api_listen);
            RegisterResponse res;
            ClientContext ctx;
            auto status = stub->RegisterServer(&ctx, req, &res);
            if (status.ok()) {
                cerr << "[server " << server_id << "] Registered. Partition "
                     << res.partition_id() << " of " << res.num_partitions() << endl;
                break;
            }
            cerr << "[server] Manager not ready, retrying..." << endl;
            this_thread::sleep_for(chrono::milliseconds(500));
        }
    }

    KvStoreService service(backer_path);
    ServerBuilder builder;
    builder.AddListeningPort(api_listen, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);

    auto server = builder.BuildAndStart();
    cerr << "[server " << server_id << "] Listening on " << api_listen << endl;
    server->Wait();
    return 0;
}
