#pragma once

#include <grpcpp/grpcpp.h>

#include <chrono>
#include <string>

namespace common {

// Generates a new, short random trace id — created once per
// incoming request, at the system's entry point (Gateway).
std::string GenerateTraceId();

// Reads the trace id an incoming gRPC call was tagged with, via
// metadata. Returns "unknown" if the call somehow arrived without
// one (e.g. called directly, bypassing the normal entry point).
std::string ExtractTraceId(const grpc::ServerContext& context);

// Attaches a trace id to an outgoing gRPC call, so the receiving
// service can read it back via ExtractTraceId().
void AttachTraceId(grpc::ClientContext* context, const std::string& trace_id);

// Logs one stage's timing in a format every service uses
// consistently, so log lines from different processes can be
// correlated by trace_id.
void LogStage(const std::string& trace_id, const std::string& service, const std::string& stage,
              std::chrono::steady_clock::duration duration);

}  // namespace common
