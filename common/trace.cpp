#include "trace.h"

#include <cstdio>
#include <iomanip>
#include <random>
#include <sstream>

namespace common {

namespace {
constexpr char kTraceIdMetadataKey[] = "trace-id";
}  // namespace

std::string GenerateTraceId() {
  // thread_local so concurrent requests (Gateway serves multithreaded)
  // never share a generator, avoiding the need for a mutex.
  thread_local std::mt19937_64 rng(std::random_device{}());
  uint64_t value = rng();

  std::ostringstream oss;
  oss << std::hex << std::setw(16) << std::setfill('0') << value;
  return oss.str();
}

std::string ExtractTraceId(const grpc::ServerContext& context) {
  const auto& metadata = context.client_metadata();
  auto it = metadata.find(kTraceIdMetadataKey);
  if (it == metadata.end()) {
    return "unknown";
  }
  return std::string(it->second.data(), it->second.size());
}

void AttachTraceId(grpc::ClientContext* context, const std::string& trace_id) {
  context->AddMetadata(kTraceIdMetadataKey, trace_id);
}

void LogStage(const std::string& trace_id, const std::string& service, const std::string& stage,
              std::chrono::steady_clock::duration duration) {
  double ms = std::chrono::duration<double, std::milli>(duration).count();
  std::printf("[%s] %s: %s took %.2fms\n", trace_id.c_str(), service.c_str(), stage.c_str(), ms);
  std::fflush(stdout);
}

}  // namespace common
