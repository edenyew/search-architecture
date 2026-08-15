#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <grpcpp/grpcpp.h>
#include <sw/redis++/redis++.h>

#include <chrono>
#include <cstdio>
#include <memory>
#include <optional>
#include <string>

#include "cache.grpc.pb.h"
#include "common.pb.h"
#include "trace.h"

namespace {

// Combines the query into one string key, e.g. "protein:10". Both
// fields are needed since the same text with a different top_k is a
// genuinely different cached result set.
std::string BuildKey(const search::common::SearchQuery& query) {
  return query.query_text() + ":" + std::to_string(query.top_k());
}

class CacheServiceImpl final : public search::cache::Cache::Service {
 public:
  explicit CacheServiceImpl(const std::string& redis_uri) : redis_(redis_uri) {}

  grpc::Status Get(grpc::ServerContext* context, const search::common::SearchQuery* request, search::cache::GetResponse* response) override {
    const std::string trace_id = common::ExtractTraceId(*context);
    const auto start = std::chrono::steady_clock::now();

    std::string key = BuildKey(*request);
    std::optional<std::string> value = redis_.get(key);

    if (!value) {
      response->set_hit(false);
      common::LogStage(trace_id, "cache", "get (miss)", std::chrono::steady_clock::now() - start);
      return grpc::Status::OK;
    }

    search::cache::GetResponse cached;
    if (!cached.ParseFromString(*value)) {
      return grpc::Status(grpc::StatusCode::INTERNAL, "failed to parse cached entry");
    }
    response->set_hit(true);
    *response->mutable_documents() = cached.documents();
    common::LogStage(trace_id, "cache", "get (hit)", std::chrono::steady_clock::now() - start);
    return grpc::Status::OK;
  }

  grpc::Status Put(grpc::ServerContext* context, const search::cache::PutRequest* request, search::cache::PutResponse* response) override {
    const std::string trace_id = common::ExtractTraceId(*context);
    const auto start = std::chrono::steady_clock::now();

    std::string key = BuildKey(request->query());

    search::cache::GetResponse wrapper;
    wrapper.set_hit(true);
    *wrapper.mutable_documents() = request->documents();

    std::string bytes;
    if (!wrapper.SerializeToString(&bytes)) {
      return grpc::Status(grpc::StatusCode::INTERNAL, "failed to serialize cache entry");
    }
    redis_.set(key, bytes);

    response->set_ok(true);
    common::LogStage(trace_id, "cache", "put", std::chrono::steady_clock::now() - start);
    return grpc::Status::OK;
  }

 private:
  sw::redis::Redis redis_;
};

}  // namespace

int main() {
  const std::string redis_uri = "tcp://127.0.0.1:6379";
  const std::string server_address = "0.0.0.0:50052";

  CacheServiceImpl service(redis_uri);

  grpc::EnableDefaultHealthCheckService(true);
  grpc::reflection::InitProtoReflectionServerBuilderPlugin();

  grpc::ServerBuilder builder;
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  builder.RegisterService(&service);

  std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
  std::printf("cache service listening on %s (redis at %s)\n", server_address.c_str(), redis_uri.c_str());
  server->Wait();

  return 0;
}
