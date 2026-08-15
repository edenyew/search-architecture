#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <grpcpp/grpcpp.h>

#include <chrono>
#include <cstdio>
#include <memory>
#include <string>

#include "common.pb.h"
#include "reranker.grpc.pb.h"
#include "trace.h"

namespace {

class RerankerServiceImpl final : public search::reranker::Reranker::Service {
 public:
  grpc::Status Rerank(grpc::ServerContext* context, const search::reranker::RerankRequest* request, search::reranker::RerankResponse* response) override {
    const std::string trace_id = common::ExtractTraceId(*context);
    const auto start = std::chrono::steady_clock::now();

    // Phase 2: deliberate passthrough. Reranker exists as a
    // complete architectural seam — Controller genuinely calls a
    // fourth downstream service — without adding new ranking logic
    // yet. Real re-ranking (e.g. an exact-phrase-match bonus, or a
    // cross-encoder second-stage model) is future work.
    *response->mutable_documents() = request->candidates();

    common::LogStage(trace_id, "reranker", "rerank", std::chrono::steady_clock::now() - start);
    return grpc::Status::OK;
  }
};

}  // namespace

int main() {
  const std::string server_address = "0.0.0.0:50053";

  RerankerServiceImpl service;

  grpc::EnableDefaultHealthCheckService(true);
  grpc::reflection::InitProtoReflectionServerBuilderPlugin();

  grpc::ServerBuilder builder;
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  builder.RegisterService(&service);

  std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
  std::printf("reranker service listening on %s\n", server_address.c_str());
  server->Wait();

  return 0;
}
