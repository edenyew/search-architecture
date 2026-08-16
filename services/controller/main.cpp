#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <grpcpp/grpcpp.h>

#include <chrono>
#include <cstdio>
#include <memory>
#include <string>

#include "cache.grpc.pb.h"
#include "circuit_breaker.h"
#include "common.pb.h"
#include "controller.grpc.pb.h"
#include "health.grpc.pb.h"
#include "reranker.grpc.pb.h"
#include "retrieval.grpc.pb.h"
#include "trace.h"

namespace {

class ControllerServiceImpl final : public search::controller::Controller::Service {
 public:
  ControllerServiceImpl(const std::shared_ptr<grpc::Channel>& retrieval_channel, const std::shared_ptr<grpc::Channel>& cache_channel,
                        const std::shared_ptr<grpc::Channel>& reranker_channel)
      : retrieval_stub_(search::retrieval::Retrieval::NewStub(retrieval_channel)),
        retrieval_health_stub_(grpc::health::v1::Health::NewStub(retrieval_channel)),
        cache_stub_(search::cache::Cache::NewStub(cache_channel)),
        reranker_stub_(search::reranker::Reranker::NewStub(reranker_channel)),
        retrieval_breaker_(/*failure_threshold=*/3, /*reset_timeout=*/std::chrono::milliseconds(5000)) {}

  grpc::Status Search(grpc::ServerContext* context, const search::common::SearchQuery* request, search::controller::SearchResponse* response) override {
    const std::string trace_id = common::ExtractTraceId(*context);
    const auto deadline = context->deadline();
    const auto start = std::chrono::steady_clock::now();

    std::string route = Classify(*request);
    grpc::Status status = Orchestrate(trace_id, deadline, *request, route, response);

    common::LogStage(trace_id, "controller", "total", std::chrono::steady_clock::now() - start);
    return status;
  }

 private:
  // Phase 1 stub: every query takes the same route. Real routing
  // logic lands here in a later phase, once there's something to
  // route between.
  std::string Classify(const search::common::SearchQuery& query) {
    return "default";
  }

  // Up to 2 retries (3 attempts total). Only UNAVAILABLE is retried —
  // the service being down/restarting is transient; other errors
  // (bad input, deadline already gone) won't be fixed by retrying.
  // Every attempt reuses the same absolute `deadline`, so once the
  // overall budget is gone, a retry just fails immediately with
  // DEADLINE_EXCEEDED (non-retryable) instead of looping forever.
  static constexpr int kMaxAttempts = 3;

  grpc::Status CallRetrievalWithRetry(const std::string& trace_id, const std::chrono::system_clock::time_point& deadline, const search::common::SearchQuery& query,
                                      search::retrieval::SearchResponse* retrieval_response) {
    grpc::Status status;
    for (int attempt = 1; attempt <= kMaxAttempts; ++attempt) {
      const auto attempt_start = std::chrono::steady_clock::now();

      grpc::ClientContext retrieval_context;
      common::AttachTraceId(&retrieval_context, trace_id);
      retrieval_context.set_deadline(deadline);
      status = retrieval_stub_->Search(&retrieval_context, query, retrieval_response);

      common::LogStage(trace_id, "controller", "retrieval_attempt_" + std::to_string(attempt), std::chrono::steady_clock::now() - attempt_start);

      if (status.ok() || status.error_code() != grpc::StatusCode::UNAVAILABLE) {
        return status;
      }
    }
    return status;
  }

  // Cheap ping used only to test recovery during the breaker's
  // Half-Open trial — a dedicated Check() call rather than firing a
  // real (possibly expensive) search request to find out whether
  // Retrieval is back. Uses its own short deadline, independent of
  // the caller's request budget, since a health probe should be fast
  // or fail fast.
  bool IsRetrievalHealthy(const std::string& trace_id) {
    grpc::ClientContext health_context;
    common::AttachTraceId(&health_context, trace_id);
    health_context.set_deadline(std::chrono::system_clock::now() + std::chrono::milliseconds(500));

    grpc::health::v1::HealthCheckRequest request;
    grpc::health::v1::HealthCheckResponse response;
    grpc::Status status = retrieval_health_stub_->Check(&health_context, request, &response);
    const bool healthy = status.ok() && response.status() == grpc::health::v1::HealthCheckResponse::SERVING;

    std::printf("[%s] controller: retrieval half-open health probe -> %s\n", trace_id.c_str(), healthy ? "healthy" : "unhealthy");
    std::fflush(stdout);
    return healthy;
  }

  // Guards CallRetrievalWithRetry with the circuit breaker: if
  // Retrieval has been failing repeatedly, fail immediately without
  // attempting (or retrying) the call at all, instead of waiting out
  // a timeout on every single request while it's down.
  grpc::Status CallRetrievalWithCircuitBreaker(const std::string& trace_id, const std::chrono::system_clock::time_point& deadline,
                                               const search::common::SearchQuery& query, search::retrieval::SearchResponse* retrieval_response) {
    if (!retrieval_breaker_.AllowRequest()) {
      std::printf("[%s] controller: retrieval circuit breaker OPEN — short-circuiting, no call attempted\n", trace_id.c_str());
      std::fflush(stdout);
      return grpc::Status(grpc::StatusCode::UNAVAILABLE, "circuit breaker open for retrieval");
    }

    // AllowRequest() just transitioned Open -> HalfOpen if this call is
    // the trial. Probe with a cheap health check first — if Retrieval
    // is still down, don't waste a full retry sequence finding that out.
    if (retrieval_breaker_.StateName() == "half_open" && !IsRetrievalHealthy(trace_id)) {
      retrieval_breaker_.RecordFailure();
      return grpc::Status(grpc::StatusCode::UNAVAILABLE, "circuit breaker half-open probe failed health check");
    }

    grpc::Status status = CallRetrievalWithRetry(trace_id, deadline, query, retrieval_response);

    if (status.ok()) {
      retrieval_breaker_.RecordSuccess();
    } else {
      retrieval_breaker_.RecordFailure();
    }

    return status;
  }

  // Cache-aside: check Cache first. On a hit, return immediately and
  // skip Retrieval and Reranker entirely — the cached value is
  // already the final, reranked result. On a miss (or if Cache
  // itself is unreachable), fall through to Retrieval, rerank, then
  // write the FINAL result back so the next identical query hits
  // without needing to rerank again.
  grpc::Status Orchestrate(const std::string& trace_id, const std::chrono::system_clock::time_point& deadline, const search::common::SearchQuery& query,
                           const std::string& route, search::controller::SearchResponse* response) {
    auto stage_start = std::chrono::steady_clock::now();
    grpc::ClientContext cache_get_context;
    common::AttachTraceId(&cache_get_context, trace_id);
    cache_get_context.set_deadline(deadline);
    search::cache::GetResponse cache_response;
    grpc::Status cache_status = cache_stub_->Get(&cache_get_context, query, &cache_response);
    common::LogStage(trace_id, "controller", "cache_get", std::chrono::steady_clock::now() - stage_start);

    if (cache_status.ok() && cache_response.hit()) {
      *response->mutable_documents() = cache_response.documents();
      return grpc::Status::OK;
    }

    stage_start = std::chrono::steady_clock::now();
    search::retrieval::SearchResponse retrieval_response;
    grpc::Status status = CallRetrievalWithCircuitBreaker(trace_id, deadline, query, &retrieval_response);
    common::LogStage(trace_id, "controller", "retrieval_call_total", std::chrono::steady_clock::now() - stage_start);
    if (!status.ok()) {
      return status;
    }

    // Rerank the candidates. If Reranker itself is unreachable,
    // gracefully degrade to the un-reranked Retrieval results rather
    // than failing the whole request.
    stage_start = std::chrono::steady_clock::now();
    grpc::ClientContext rerank_context;
    common::AttachTraceId(&rerank_context, trace_id);
    rerank_context.set_deadline(deadline);
    search::reranker::RerankRequest rerank_request;
    *rerank_request.mutable_query() = query;
    *rerank_request.mutable_candidates() = retrieval_response.documents();
    search::reranker::RerankResponse rerank_response;
    grpc::Status rerank_status = reranker_stub_->Rerank(&rerank_context, rerank_request, &rerank_response);
    common::LogStage(trace_id, "controller", "rerank_call", std::chrono::steady_clock::now() - stage_start);

    if (rerank_status.ok()) {
      *response->mutable_documents() = rerank_response.documents();
    } else {
      *response->mutable_documents() = retrieval_response.documents();
    }

    stage_start = std::chrono::steady_clock::now();
    grpc::ClientContext cache_put_context;
    common::AttachTraceId(&cache_put_context, trace_id);
    cache_put_context.set_deadline(deadline);
    search::cache::PutRequest put_request;
    *put_request.mutable_query() = query;
    *put_request.mutable_documents() = response->documents();
    search::cache::PutResponse put_response;
    cache_stub_->Put(&cache_put_context, put_request, &put_response);
    common::LogStage(trace_id, "controller", "cache_put", std::chrono::steady_clock::now() - stage_start);

    return grpc::Status::OK;
  }

  std::unique_ptr<search::retrieval::Retrieval::Stub> retrieval_stub_;
  std::unique_ptr<grpc::health::v1::Health::Stub> retrieval_health_stub_;
  std::unique_ptr<search::cache::Cache::Stub> cache_stub_;
  std::unique_ptr<search::reranker::Reranker::Stub> reranker_stub_;
  controller::CircuitBreaker retrieval_breaker_;
};

}  // namespace

int main() {
  const std::string retrieval_address = "localhost:50051";
  const std::string cache_address = "localhost:50052";
  const std::string reranker_address = "localhost:50053";
  const std::string server_address = "0.0.0.0:50050";

  std::shared_ptr<grpc::Channel> retrieval_channel = grpc::CreateChannel(retrieval_address, grpc::InsecureChannelCredentials());
  std::shared_ptr<grpc::Channel> cache_channel = grpc::CreateChannel(cache_address, grpc::InsecureChannelCredentials());
  std::shared_ptr<grpc::Channel> reranker_channel = grpc::CreateChannel(reranker_address, grpc::InsecureChannelCredentials());
  ControllerServiceImpl service(retrieval_channel, cache_channel, reranker_channel);

  grpc::EnableDefaultHealthCheckService(true);
  grpc::reflection::InitProtoReflectionServerBuilderPlugin();

  grpc::ServerBuilder builder;
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  builder.RegisterService(&service);

  std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
  std::printf("controller service listening on %s (retrieval at %s, cache at %s, reranker at %s)\n", server_address.c_str(), retrieval_address.c_str(),
              cache_address.c_str(), reranker_address.c_str());
  server->Wait();

  return 0;
}
